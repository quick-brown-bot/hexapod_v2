#include "rs485_master.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "rs485_master";

// --- Physical layer (see docs/architecture/HARDWARE_AND_MECHANICS.md) ---
#define RS485_UART_PORT    UART_NUM_2
#define RS485_TX_GPIO      17   // SP3485 DI
#define RS485_RX_GPIO      16   // SP3485 RO
#define RS485_DE_GPIO      4    // SP3485 DE/RE, driven as UART2 RTS in RS485 half-duplex mode
#define RS485_BAUD         1000000

#define RS485_RX_BUF_SIZE  256
#define RS485_TX_BUF_SIZE  256

// Per-leg read timeout. A full transaction is well under 1 ms on the wire;
// this allows for RP2040 processing latency while staying within the 10 ms
// six-leg budget.
#define RS485_RESPONSE_TIMEOUT_MS  3

#define RS485_LINE_MAX     128
#define RS485_PARAM_SLOTS  0x10  // param IDs are 0x01..0x09; index directly

// --- Shared state ---------------------------------------------------------
// Written by the motion loop (set_leg_angles / queue_param / request_positions)
// and the master task (telemetry). Guarded by s_lock.

typedef struct {
    float coxa_deg;
    float femur_deg;
    float tibia_deg;
    bool  param_pending[RS485_PARAM_SLOTS];
    int32_t param_value[RS485_PARAM_SLOTS];
    bool  request_positions;
} leg_command_t;

static leg_command_t   s_cmd[NUM_LEGS];
static leg_telemetry_t s_tlm[NUM_LEGS];
static SemaphoreHandle_t s_lock;
static bool s_started = false;

// --- CRC8 -----------------------------------------------------------------
// CRC-8/SMBus: polynomial 0x07, init 0x00, no reflection, no final XOR.
// Covers all bytes from the leading '>' or '<' up to (not including) '*'.
// The RP2040 leg firmware MUST use the identical algorithm.
static uint8_t crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

// --- Frame assembly -------------------------------------------------------
// Builds a pull frame for leg `idx` (0-based) into `out`. Returns frame length.
// Snapshots the per-leg command under the lock first so the wire formatting is
// done outside the critical section.
static int build_pull_frame(int idx, char *out, size_t out_sz)
{
    leg_command_t snap;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    snap = s_cmd[idx];
    xSemaphoreGive(s_lock);

    uint8_t flags = snap.request_positions ? 0x01 : 0x00;

    // Body without CRC: ">addr,flags,coxa,femur,tibia[,Pii=vvv...]"
    int n = snprintf(out, out_sz, ">%d,%02X,%.1f,%.1f,%.1f",
                     idx + 1, flags, snap.coxa_deg, snap.femur_deg, snap.tibia_deg);
    if (n < 0 || (size_t)n >= out_sz) return -1;

    for (int p = 0; p < RS485_PARAM_SLOTS; ++p) {
        if (snap.param_pending[p]) {
            int m = snprintf(out + n, out_sz - n, ",P%02X=%ld", p, (long)snap.param_value[p]);
            if (m < 0 || (size_t)(n + m) >= out_sz) return -1;
            n += m;
        }
    }

    uint8_t crc = crc8((const uint8_t *)out, (size_t)n);
    int m = snprintf(out + n, out_sz - n, "*%02X\n", crc);
    if (m < 0 || (size_t)(n + m) >= out_sz) return -1;
    n += m;
    return n;
}

// --- Response parsing -----------------------------------------------------
// Parses a telemetry response line (NUL-terminated, may include trailing '\n').
// Validates leading '<', CRC, and address. On success writes into *out and
// returns true.
static bool parse_response(int idx, char *line, leg_telemetry_t *out)
{
    if (line[0] != '<') return false;

    char *star = strchr(line, '*');
    if (!star) return false;

    // CRC covers '<' .. byte before '*'.
    size_t body_len = (size_t)(star - line);
    uint8_t want = crc8((const uint8_t *)line, body_len);
    uint8_t got = (uint8_t)strtoul(star + 1, NULL, 16);
    if (want != got) {
        ESP_LOGD(TAG, "leg %d CRC mismatch want=%02X got=%02X", idx + 1, want, got);
        return false;
    }

    // Tokenize the body between '<' and '*'.
    *star = '\0';
    char *p = line + 1; // skip '<'

    // Fields: addr,status,itot,icoxa,ifemur,itibia[,coxa,femur,tibia]
    char *tok[9];
    int ntok = 0;
    for (char *s = strtok(p, ","); s && ntok < 9; s = strtok(NULL, ",")) {
        tok[ntok++] = s;
    }
    if (ntok != 6 && ntok != 9) return false;

    int addr = atoi(tok[0]);
    if (addr != idx + 1) return false;

    leg_telemetry_t t = {0};
    t.valid = true;
    t.stale = false;
    t.status = (uint8_t)strtoul(tok[1], NULL, 16);
    t.current_total_ma = (uint16_t)atoi(tok[2]);
    t.current_coxa_ma  = (uint16_t)atoi(tok[3]);
    t.current_femur_ma = (uint16_t)atoi(tok[4]);
    t.current_tibia_ma = (uint16_t)atoi(tok[5]);
    if (ntok == 9) {
        t.has_positions = true;
        t.pos_coxa_deg  = strtof(tok[6], NULL);
        t.pos_femur_deg = strtof(tok[7], NULL);
        t.pos_tibia_deg = strtof(tok[8], NULL);
    }
    t.last_update_us = esp_timer_get_time();
    *out = t;
    return true;
}

// Reads one '\n'-terminated line from the bus into `buf`, with an overall
// deadline. Returns the line length (excluding NUL), or -1 on timeout/overflow.
static int read_line(char *buf, size_t buf_sz, int timeout_ms)
{
    int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    size_t len = 0;
    while (esp_timer_get_time() < deadline) {
        uint8_t c;
        int r = uart_read_bytes(RS485_UART_PORT, &c, 1, pdMS_TO_TICKS(1));
        if (r == 1) {
            if (c == '\n') {
                buf[len] = '\0';
                return (int)len;
            }
            if (len + 1 < buf_sz) {
                buf[len++] = (char)c;
            } else {
                return -1; // overflow
            }
        }
    }
    return -1; // timeout
}

// Runs one full request-response transaction with leg `idx` (0-based).
static void poll_leg(int idx)
{
    char frame[RS485_LINE_MAX];
    int flen = build_pull_frame(idx, frame, sizeof(frame));
    if (flen < 0) {
        ESP_LOGE(TAG, "leg %d frame build failed", idx + 1);
        return;
    }

    // Flush any stale RX bytes before transmitting.
    uart_flush_input(RS485_UART_PORT);

    // In RS485 half-duplex mode the driver asserts DE (RTS) during TX and
    // releases it in hardware after the stop bit. Wait for TX completion
    // before reading so we don't capture our own transmission.
    uart_write_bytes(RS485_UART_PORT, frame, (size_t)flen);
    uart_wait_tx_done(RS485_UART_PORT, pdMS_TO_TICKS(5));

    char line[RS485_LINE_MAX];
    int llen = read_line(line, sizeof(line), RS485_RESPONSE_TIMEOUT_MS);

    leg_telemetry_t t;
    bool ok = (llen > 0) && parse_response(idx, line, &t);

    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (ok) {
        s_tlm[idx] = t;
        // Successful response implicitly ACKs any config carried in this pull.
        for (int p = 0; p < RS485_PARAM_SLOTS; ++p) {
            s_cmd[idx].param_pending[p] = false;
        }
        s_cmd[idx].request_positions = false;
    } else {
        // Mark stale; leave pending config queued for the next pull.
        s_tlm[idx].stale = true;
    }
    xSemaphoreGive(s_lock);

    if (!ok) {
        ESP_LOGD(TAG, "leg %d no/invalid response", idx + 1);
    }
}

static void rs485_master_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "RS485 master task started (UART%d, %d baud)", RS485_UART_PORT, RS485_BAUD);
    while (1) {
        for (int i = 0; i < NUM_LEGS; ++i) {
            poll_leg(i);
        }
        // Yield briefly so a full sweep does not starve lower-priority tasks.
        // A sweep is ~4-6 ms; one tick keeps the cadence near the 100 Hz loop.
        vTaskDelay(1);
    }
}

// --- Public API -----------------------------------------------------------

esp_err_t rs485_master_init(void)
{
    if (s_started) return ESP_OK;

    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return ESP_ERR_NO_MEM;

    memset(s_cmd, 0, sizeof(s_cmd));
    memset(s_tlm, 0, sizeof(s_tlm));

    uart_config_t cfg = {
        .baud_rate = RS485_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err;
    err = uart_driver_install(RS485_UART_PORT, RS485_RX_BUF_SIZE, RS485_TX_BUF_SIZE, 0, NULL, 0);
    if (err != ESP_OK) { ESP_LOGE(TAG, "uart_driver_install: %s", esp_err_to_name(err)); return err; }
    err = uart_param_config(RS485_UART_PORT, &cfg);
    if (err != ESP_OK) { ESP_LOGE(TAG, "uart_param_config: %s", esp_err_to_name(err)); return err; }
    err = uart_set_pin(RS485_UART_PORT, RS485_TX_GPIO, RS485_RX_GPIO, RS485_DE_GPIO, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) { ESP_LOGE(TAG, "uart_set_pin: %s", esp_err_to_name(err)); return err; }
    // Hardware-driven DE on the RTS line, de-asserted automatically after the
    // stop bit. Avoids the manual DE turnaround race at 1 Mbps.
    err = uart_set_mode(RS485_UART_PORT, UART_MODE_RS485_HALF_DUPLEX);
    if (err != ESP_OK) { ESP_LOGE(TAG, "uart_set_mode: %s", esp_err_to_name(err)); return err; }

    BaseType_t ok = xTaskCreate(rs485_master_task, "rs485_master", 4096, NULL, 9, NULL);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "failed to create RS485 master task");
        return ESP_ERR_NO_MEM;
    }

    s_started = true;
    return ESP_OK;
}

void rs485_master_set_leg_angles(int leg, float coxa_deg, float femur_deg, float tibia_deg)
{
    if (leg < 0 || leg >= NUM_LEGS) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_cmd[leg].coxa_deg  = coxa_deg;
    s_cmd[leg].femur_deg = femur_deg;
    s_cmd[leg].tibia_deg = tibia_deg;
    xSemaphoreGive(s_lock);
}

bool rs485_master_get_telemetry(int leg, leg_telemetry_t *out)
{
    if (leg < 0 || leg >= NUM_LEGS || !out) return false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    leg_telemetry_t t = s_tlm[leg];
    xSemaphoreGive(s_lock);
    if (!t.valid) return false;
    *out = t;
    return true;
}

void rs485_master_queue_param(int leg, rs485_param_id_t param, int32_t value)
{
    if (leg < 0 || leg >= NUM_LEGS) return;
    if ((int)param < 0 || (int)param >= RS485_PARAM_SLOTS) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_cmd[leg].param_pending[param] = true;
    s_cmd[leg].param_value[param] = value;
    xSemaphoreGive(s_lock);
}

void rs485_master_request_positions(int leg)
{
    if (leg < 0 || leg >= NUM_LEGS) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_cmd[leg].request_positions = true;
    xSemaphoreGive(s_lock);
}
