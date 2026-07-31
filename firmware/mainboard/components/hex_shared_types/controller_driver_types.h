/*
 * Shared controller driver type enum used across modules.
 */

#ifndef HEXAPOD_CONTROLLER_DRIVER_TYPES_H
#define HEXAPOD_CONTROLLER_DRIVER_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CONTROLLER_DRIVER_FLYSKY_IBUS = 0,
    CONTROLLER_DRIVER_UART_GENERIC,
    CONTROLLER_DRIVER_BT_CLASSIC,
    CONTROLLER_DRIVER_BT_LE,
    CONTROLLER_DRIVER_WIFI_TCP,
} controller_driver_type_e;

#ifdef __cplusplus
}
#endif

#endif // HEXAPOD_CONTROLLER_DRIVER_TYPES_H
