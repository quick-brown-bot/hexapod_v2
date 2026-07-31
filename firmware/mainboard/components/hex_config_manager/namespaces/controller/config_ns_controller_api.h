/*
 * Public controller namespace types and APIs.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "controller_driver_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    controller_driver_type_e driver_type;
    uint32_t task_stack;
    uint32_t task_priority;
    float stick_deadband;

    // Failsafe command profile (used when no controller data is available)
    float failsafe_vx;
    float failsafe_wz;
    float failsafe_z_target;
    float failsafe_y_offset;
    float failsafe_step_scale;
    int32_t failsafe_gait;
    bool failsafe_enable;
    bool failsafe_pose_mode;
    bool failsafe_terrain_climb;

    // FlySky iBUS driver options
    int32_t flysky_uart_port;
    int32_t flysky_tx_gpio;
    int32_t flysky_rx_gpio;
    int32_t flysky_rts_gpio;
    int32_t flysky_cts_gpio;
    int32_t flysky_baud_rate;
} controller_config_namespace_t;

const controller_config_namespace_t* config_get_controller(void);
esp_err_t config_set_controller(const controller_config_namespace_t* config);

void config_load_controller_defaults(controller_config_namespace_t* config);

#ifdef __cplusplus
}
#endif
