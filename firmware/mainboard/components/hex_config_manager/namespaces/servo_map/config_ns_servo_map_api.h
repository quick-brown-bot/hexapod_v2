/*
 * Public servo map namespace types and APIs.
 */

#pragma once

#include <stdint.h>

#include "config_ns_joint_api.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int32_t servo_gpio[NUM_LEGS][NUM_JOINTS_PER_LEG];
    int32_t mcpwm_group_id[NUM_LEGS];
    int32_t servo_driver_sel[NUM_LEGS][NUM_JOINTS_PER_LEG];
} servo_map_config_t;

const servo_map_config_t* config_get_servo_map(void);
esp_err_t config_set_servo_map(const servo_map_config_t* config);

void config_load_servo_map_defaults(servo_map_config_t* config);

#ifdef __cplusplus
}
#endif
