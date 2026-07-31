/*
 * Public motion limits namespace types and APIs.
 */

#pragma once

#include "config_ns_joint_api.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float max_velocity[NUM_JOINTS_PER_LEG];
    float max_acceleration[NUM_JOINTS_PER_LEG];
    float max_jerk[NUM_JOINTS_PER_LEG];

    float velocity_filter_alpha;
    float accel_filter_alpha;
    float leg_velocity_filter_alpha;
    float body_velocity_filter_alpha;
    float body_pitch_filter_alpha;
    float body_roll_filter_alpha;

    float front_to_back_distance;
    float left_to_right_distance;
    float max_leg_velocity;
    float max_body_velocity;
    float max_angular_velocity;
    float min_dt;
    float max_dt;

    float body_offset_x;
    float body_offset_y;
    float body_offset_z;
} motion_limits_config_t;

const motion_limits_config_t* config_get_motion_limits(void);
esp_err_t config_set_motion_limits(const motion_limits_config_t* config);

void config_load_motion_limits_defaults(motion_limits_config_t* config);

#ifdef __cplusplus
}
#endif
