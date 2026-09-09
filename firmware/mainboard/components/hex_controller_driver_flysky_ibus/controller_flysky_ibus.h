#ifndef CONTROLLER_FLYSKY_IBUS_H
#define CONTROLLER_FLYSKY_IBUS_H

#include <stdint.h>
#include <stdbool.h>
#include "controller.h"

#ifdef __cplusplus
extern "C" {
#endif

// Configuration for FlySky iBUS driver
// Keep small & POD so it can be copied or placed in NVS-friendly storage.
typedef struct {
    int uart_port;   // UART_NUM_x value
    int tx_gpio;     // TX pin (or -1 for no change)
    int rx_gpio;     // RX pin
    int rts_gpio;    // or -1
    int cts_gpio;    // or -1
    int baud_rate;   // e.g., 115200
} controller_flysky_ibus_cfg_t;

// Convenience fallback initializer, used only if the bootstrap flow fails to
// hand this driver its config. The authoritative values live in the
// `controller` config namespace (config_domain_controller_defaults.c); keep
// this mirror in sync with it -- notably rx_gpio = GPIO5.
static inline controller_flysky_ibus_cfg_t controller_flysky_ibus_default(void) {
    controller_flysky_ibus_cfg_t c = {
        .uart_port = 1,   // UART_NUM_1
        .tx_gpio = -1,
        .rx_gpio = 5,
        .rts_gpio = -1,
        .cts_gpio = -1,
        .baud_rate = 115200,
    };
    return c;
}

// Driver initialization function (called by controller bootstrap flow)
void controller_driver_init_flysky_ibus(const controller_config_t *cfg);

#ifdef __cplusplus
}
#endif

#endif // CONTROLLER_FLYSKY_IBUS_H
