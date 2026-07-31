/*
 * Public gait namespace types and APIs.
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float cycle_time_s;
    float step_length_m;
    float clearance_height_m;
    float y_range_m;
    float z_min_m;
    float z_max_m;
    float max_yaw_per_cycle_rad;
    float turn_direction;
} gait_config_t;

const gait_config_t* config_get_gait(void);
esp_err_t config_set_gait(const gait_config_t* config);

void config_load_gait_defaults(gait_config_t* config);

#ifdef __cplusplus
}
#endif
