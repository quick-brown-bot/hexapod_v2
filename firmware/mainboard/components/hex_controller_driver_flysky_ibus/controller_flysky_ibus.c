// FlySky iBUS driver implementation using UART; pushes channel updates
// into controller abstraction via controller_internal helpers.
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "controller_internal.h"
#include "controller_flysky_ibus.h"
#include <math.h>

#define IBUS_BUF_SIZE 64
#define IBUS_FRAME_LEN 32
static const char *TAG = "ctrl_flysky_ibus";

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

static void flysky_task(void *arg)
{
    (void)controller_internal_get_config(); // currently unused core params
    // Attempt to fetch driver specific config, else fall back to internal defaults
    controller_flysky_ibus_cfg_t local_cfg;
    const controller_flysky_ibus_cfg_t *cfg_drv = NULL;
    size_t sz = 0;
    const void *p = controller_internal_get_driver_cfg(&sz);
    if (p && sz == sizeof(controller_flysky_ibus_cfg_t)) {
        cfg_drv = (const controller_flysky_ibus_cfg_t *)p;
    } else {
        local_cfg = controller_flysky_ibus_default();
        cfg_drv = &local_cfg;
    }
    // Configure UART driver
    uart_config_t uart_config = {
        .baud_rate = cfg_drv->baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    int intr_alloc_flags = 0;
    ESP_ERROR_CHECK(uart_driver_install(cfg_drv->uart_port, 1024, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(cfg_drv->uart_port, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(cfg_drv->uart_port, cfg_drv->tx_gpio, cfg_drv->rx_gpio, cfg_drv->rts_gpio, cfg_drv->cts_gpio));

    uint8_t data[IBUS_BUF_SIZE];
    TickType_t last_frame_tick = xTaskGetTickCount();
    controller_internal_set_connected(false);
    int channels = min(CONTROLLER_MAX_CHANNELS, 14); // iBUS provides up to 14 channels

    // Standard iBUS frame: 0x20 0x40, then 14 little-endian u16 channels, then
    // a little-endian u16 checksum = 0xFFFF - sum(first 30 bytes). 32 bytes,
    // ~one every 7 ms. A UART read almost never lands on a frame boundary, so
    // feed bytes through a small resync state machine rather than assuming
    // data[0] is the header.
    uint8_t frame[IBUS_FRAME_LEN];
    int fill = 0;

    while (1) {
        const TickType_t timeout_ticks = pdMS_TO_TICKS(20);
        int len = uart_read_bytes(cfg_drv->uart_port, data, sizeof(data), timeout_ticks);
        TickType_t now_tick = xTaskGetTickCount();

        for (int k = 0; k < len; ++k) {
            uint8_t b = data[k];
            if (fill == 0) {
                if (b == 0x20) frame[fill++] = b;
            } else if (fill == 1) {
                if (b == 0x40) frame[fill++] = b;
                else fill = (b == 0x20) ? 1 : 0;  // re-anchor on a fresh 0x20
            } else {
                frame[fill++] = b;
                if (fill < IBUS_FRAME_LEN) continue;
                fill = 0;

                uint16_t sum = 0;
                for (int i = 0; i < IBUS_FRAME_LEN - 2; ++i) sum += frame[i];
                uint16_t want = (uint16_t)0xFFFF - sum;
                uint16_t got = (uint16_t)(frame[30] | (frame[31] << 8));
                if (want != got) continue;  // corrupt frame -- wait for the next

                int16_t local[CONTROLLER_MAX_CHANNELS];
                for (int i = 0; i < channels; ++i) {
                    uint16_t raw = (uint16_t)(frame[2 + i*2] | (frame[3 + i*2] << 8)); // 1000..2000 us
                    if (raw < 1000) raw = 1000;
                    if (raw > 2000) raw = 2000;
                    float norm = ((float)raw - 1500.0f) / 500.0f; // -1..+1
                    if (norm < -1.0f) norm = -1.0f;
                    if (norm > 1.0f) norm = 1.0f;
                    int32_t sv = (int32_t)lrintf(norm * 32767.0f);
                    if (sv < -32768) sv = -32768;
                    if (sv > 32767) sv = 32767;
                    local[i] = (int16_t)sv;
                }

                controller_internal_update_channels(local);
                last_frame_tick = now_tick;
                if (!controller_internal_is_connected()) {
                    controller_internal_set_connected(true);
                    ESP_LOGI(TAG, "iBUS connected (UART%d RX=%d TX=%d)", (int)cfg_drv->uart_port, (int)cfg_drv->rx_gpio, (int)cfg_drv->tx_gpio);
                }
            }
        }
        // Timeout handling: if no frames >1s enter failsafe
        TickType_t dt_ticks = now_tick - last_frame_tick;
        if (controller_internal_is_connected() && dt_ticks > pdMS_TO_TICKS(1000)) {
            controller_internal_set_connected(false);
            ESP_LOGW(TAG, "iBUS disconnected: no frames for %u ms, entering failsafe", (unsigned)(dt_ticks * portTICK_PERIOD_MS));
            controller_internal_set_failsafe();
        }
    }
}

void controller_driver_init_flysky_ibus(const controller_config_t *cfg)
{
    (void)cfg; // already stored globally
    xTaskCreate(flysky_task, "flysky_ibus", cfg->task_stack, NULL, cfg->task_prio, NULL);
}
