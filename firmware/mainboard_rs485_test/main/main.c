// Standalone RS485 bring-up + benchmark test for the V2 mainboard <-> legboard
// link.
//
// Minimal ESP-IDF app (no gait framework, config manager, or WiFi). On boot it
// runs a battery of back-to-back polling benchmarks against leg address 1 to
// characterize real round-trip latency and the achievable transaction rate,
// then falls into a slow visual sweep so you can watch the leg move. Console
// output goes over UART0 (USB-serial), a separate peripheral from the RS485
// link on UART2, so the two never contend for the same wire.
//
// Wire pins per hardware/mainboard/mainboard_sch.py: TX=IO17, RX=IO16,
// DE=IO4 (SP3485, half-duplex, 1 Mbps). Frame format and CRC8 must stay
// byte-for-byte identical to firmware/leg/src/protocol.c — see
// docs/interfaces/RS485_PROTOCOL.md.
//
// Benchmark methodology: each transaction is timed from the start of
// uart_write_bytes() for the request to the '\n' terminating the response
// being pulled out of the RX FIFO — i.e. it includes request wire time, the
// leg's DE turnaround + processing, and response wire time. Reads are
// non-blocking busy-polls against esp_timer (microsecond resolution),
// deliberately avoiding FreeRTOS tick-based waits (default 100 Hz tick / 10ms
// granularity) which would be far too coarse to characterize a link this
// fast.

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "rs485_test";

#define RS485_UART_PORT    UART_NUM_2
#define RS485_TX_GPIO      17
#define RS485_RX_GPIO      16
#define RS485_DE_GPIO      4
#define RS485_BAUD         1000000

#define RS485_RX_BUF_SIZE  256
#define RS485_TX_BUF_SIZE  256
#define RS485_LINE_MAX     128

#define TEST_LEG_ADDR      1

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

static int build_pull_frame(char *out, size_t out_sz, int addr, uint8_t flags, float coxa, float femur, float tibia)
{
    int n = snprintf(out, out_sz, ">%d,%02X,%.1f,%.1f,%.1f", addr, flags, coxa, femur, tibia);
    if (n < 0 || (size_t)n >= out_sz) return -1;
    uint8_t crc = crc8((const uint8_t *)out, (size_t)n);
    int m = snprintf(out + n, out_sz - n, "*%02X\n", crc);
    if (m < 0 || (size_t)(n + m) >= out_sz) return -1;
    return n + m;
}

// Non-blocking busy-poll read with microsecond-resolution timeout. Returns
// line length (excluding '\n') on success, -1 on timeout/overflow.
static int read_line_busy(char *buf, size_t buf_sz, int64_t timeout_us)
{
    int64_t deadline = esp_timer_get_time() + timeout_us;
    size_t len = 0;
    while (esp_timer_get_time() < deadline) {
        uint8_t c;
        int r = uart_read_bytes(RS485_UART_PORT, &c, 1, 0); // ticks_to_wait=0: non-blocking
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

static bool validate_response(char *line)
{
    if (line[0] != '<') return false;
    char *star = strchr(line, '*');
    if (!star) return false;
    size_t body_len = (size_t)(star - line);
    uint8_t want = crc8((const uint8_t *)line, body_len);
    uint8_t got = (uint8_t)strtoul(star + 1, NULL, 16);
    return want == got;
}

typedef struct {
    uint32_t total;
    uint32_t ok;
    uint32_t crc_fail;
    uint64_t sum_us;
    uint64_t sumsq_us; // for stddev, computed on ok samples only
    uint32_t min_us;
    uint32_t max_us;
} bench_stats_t;

// Sends a handful of queries to `addr` and reports how many got a valid
// response, so we can confirm a given leg is present and answering before
// running the timing-sensitive benchmarks against it.
static void check_leg_connectivity(int addr, int n)
{
    char frame[RS485_LINE_MAX];
    char line[RS485_LINE_MAX];
    int flen = build_pull_frame(frame, sizeof(frame), addr, 0x01, 0.0f, 0.0f, 0.0f);

    int ok = 0;
    for (int i = 0; i < n; ++i) {
        uart_flush_input(RS485_UART_PORT);
        uart_write_bytes(RS485_UART_PORT, frame, (size_t)flen);
        uart_wait_tx_done(RS485_UART_PORT, pdMS_TO_TICKS(5));

        int llen = read_line_busy(line, sizeof(line), 20000);
        if (llen > 0 && validate_response(line)) {
            ok++;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    ESP_LOGI(TAG, "leg %d connectivity: %d/%d responses OK%s", addr, ok, n,
             ok == 0 ? " — NO RESPONSE, check wiring/address/power" : "");
}

// Runs `n` back-to-back transactions at a fixed neutral target with the given
// per-transaction timeout, no artificial inter-request delay. Feeds the idle
// task periodically so the watchdog doesn't trip during the busy-poll.
static void run_benchmark(const char *label, int64_t timeout_us, int n, uint8_t flags)
{
    bench_stats_t s = {0};
    s.min_us = UINT32_MAX;

    char frame[RS485_LINE_MAX];
    char line[RS485_LINE_MAX];
    int flen = build_pull_frame(frame, sizeof(frame), TEST_LEG_ADDR, flags, 0.0f, 0.0f, 0.0f);

    int64_t bench_start = esp_timer_get_time();

    for (int i = 0; i < n; ++i) {
        uart_flush_input(RS485_UART_PORT);
        int64_t t0 = esp_timer_get_time();
        uart_write_bytes(RS485_UART_PORT, frame, (size_t)flen);
        uart_wait_tx_done(RS485_UART_PORT, pdMS_TO_TICKS(5));

        int llen = read_line_busy(line, sizeof(line), timeout_us);
        s.total++;

        if (llen > 0) {
            int64_t rtt_us = esp_timer_get_time() - t0;
            if (validate_response(line)) {
                s.ok++;
                uint32_t v = (uint32_t)rtt_us;
                s.sum_us += v;
                s.sumsq_us += (uint64_t)v * (uint64_t)v;
                if (v < s.min_us) s.min_us = v;
                if (v > s.max_us) s.max_us = v;
            } else {
                s.crc_fail++;
            }
        }

        if ((i % 50) == 0) {
            vTaskDelay(1); // let IDLE run / feed the task watchdog
        }
    }

    int64_t bench_elapsed_us = esp_timer_get_time() - bench_start;

    double avg_us = s.ok ? (double)s.sum_us / s.ok : 0.0;
    double var_us = s.ok ? ((double)s.sumsq_us / s.ok - avg_us * avg_us) : 0.0;
    double stddev_us = var_us > 0.0 ? sqrt(var_us) : 0.0;
    double loss_pct = 100.0 * (double)(s.total - s.ok) / (double)s.total;
    double achieved_rate_hz = (double)s.total / ((double)bench_elapsed_us / 1e6);

    ESP_LOGI(TAG, "=== [%s] timeout=%lld us, n=%d ===", label, (long long)timeout_us, n);
    ESP_LOGI(TAG, "  ok=%lu crc_fail=%lu timeout=%lu loss=%.1f%%",
             (unsigned long)s.ok, (unsigned long)s.crc_fail,
             (unsigned long)(s.total - s.ok - s.crc_fail), loss_pct);
    if (s.ok > 0) {
        ESP_LOGI(TAG, "  RTT us: min=%lu avg=%.0f max=%lu stddev=%.0f",
                 (unsigned long)s.min_us, avg_us, (unsigned long)s.max_us, stddev_us);
    }
    ESP_LOGI(TAG, "  wall clock: %lld us total, achieved %.1f req/s (includes this loop's own overhead)",
             (long long)bench_elapsed_us, achieved_rate_hz);
}

void app_main(void)
{
    uart_config_t cfg = {
        .baud_rate = RS485_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(RS485_UART_PORT, RS485_RX_BUF_SIZE, RS485_TX_BUF_SIZE, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(RS485_UART_PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(RS485_UART_PORT, RS485_TX_GPIO, RS485_RX_GPIO, RS485_DE_GPIO, UART_PIN_NO_CHANGE));
    // Hardware-driven DE on RTS, de-asserted automatically after the stop bit.
    ESP_ERROR_CHECK(uart_set_mode(RS485_UART_PORT, UART_MODE_RS485_HALF_DUPLEX));

    ESP_LOGI(TAG, "RS485 benchmark: UART%d TX=%d RX=%d DE=%d @ %d baud, leg %d",
             RS485_UART_PORT, RS485_TX_GPIO, RS485_RX_GPIO, RS485_DE_GPIO, RS485_BAUD, TEST_LEG_ADDR);
    vTaskDelay(pdMS_TO_TICKS(500)); // let the leg settle after any recent reset

    ESP_LOGI(TAG, "=== CONNECTIVITY CHECK: leg 1 and leg 2 ===");
    check_leg_connectivity(1, 10);
    check_leg_connectivity(2, 10);

    // Phase 1: generous timeout, back-to-back, WITH position echo requested
    // (flags=0x01) — establishes the true RTT distribution for the longer
    // "worst case" response shape, with (almost) no transactions lost to an
    // overly tight timeout.
    run_benchmark("generous 20ms timeout, +positions", 20000, 500, 0x01);

    // Phase 2: production timeout (hex_rs485_master.c RS485_RESPONSE_TIMEOUT_MS = 3ms)
    // — is the real system's timeout budget actually safe?
    run_benchmark("production 3ms timeout, +positions", 3000, 500, 0x01);

    // Phase 3-5: push the timeout down to find the real floor.
    run_benchmark("tight 2ms timeout, +positions", 2000, 500, 0x01);
    run_benchmark("tight 1ms timeout, +positions", 1000, 300, 0x01);
    run_benchmark("tight 500us timeout, +positions", 500, 300, 0x01);

    // Phase 6: sustained max-rate burst — no per-transaction timeout headroom
    // concerns (generous timeout), just hammering as fast as possible for a
    // larger sample, to see the steady-state achievable rate for one leg.
    run_benchmark("sustained max rate (2000 tx), +positions", 20000, 2000, 0x01);

    // Phase 7: same but WITHOUT position echo (flags=0x00) — the shorter
    // response shape most production polls actually use (positions are
    // requested one-shot, not every cycle). Shows the frame-size sensitivity.
    run_benchmark("sustained max rate (2000 tx), no positions", 20000, 2000, 0x00);

    ESP_LOGI(TAG, "=== BENCHMARK COMPLETE — switching to slow visual sweep ===");

    float t = 0.0f;
    char frame[RS485_LINE_MAX];
    char line[RS485_LINE_MAX];
    int addrs[2] = {1, 2};

    while (1) {
        float angle = 20.0f * sinf(t);

        for (int k = 0; k < 2; ++k) {
            int addr = addrs[k];
            int flen = build_pull_frame(frame, sizeof(frame), addr, 0x01, angle, angle, angle);
            if (flen < 0) { ESP_LOGE(TAG, "frame build failed"); continue; }

            uart_flush_input(RS485_UART_PORT);
            uart_write_bytes(RS485_UART_PORT, frame, (size_t)flen);
            uart_wait_tx_done(RS485_UART_PORT, pdMS_TO_TICKS(5));

            int llen = read_line_busy(line, sizeof(line), 10000);
            if (llen > 0 && validate_response(line)) {
                ESP_LOGI(TAG, "leg %d OK: %s", addr, line);
            } else {
                ESP_LOGW(TAG, "leg %d no response (timeout) — sent: %s", addr, frame);
            }
        }

        t += 0.1f;
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
