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
    // Wilson gait continuum: swing-fraction (duty factor) bounds. Duty factor
    // is the fraction of the cycle a leg spends in swing (not stance); it
    // continuously scales with commanded speed instead of being selected
    // from 3 discrete gaits. duty_factor_min ~ old wave S (~0.1, cautious,
    // most legs on the ground), duty_factor_max ~ old tripod S (0.5, the
    // statically-stable ceiling for a hexapod: above 0.5 more than 3 legs
    // would be airborne at once).
    float duty_factor_min;
    float duty_factor_max;
} gait_config_t;

const gait_config_t* config_get_gait(void);
esp_err_t config_set_gait(const gait_config_t* config);

void config_load_gait_defaults(gait_config_t* config);

#ifdef __cplusplus
}
#endif
