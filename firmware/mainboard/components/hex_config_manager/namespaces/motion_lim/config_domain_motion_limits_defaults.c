#include "config_domain_defaults.h"

#include <string.h>

void config_load_motion_limits_defaults(motion_limits_config_t* config) {
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(motion_limits_config_t));

    config->max_velocity[0] = 5.0f;
    config->max_velocity[1] = 5.0f;
    config->max_velocity[2] = 5.0f;

    config->max_acceleration[0] = 600.0f;
    config->max_acceleration[1] = 600.0f;
    config->max_acceleration[2] = 600.0f;

    config->max_jerk[0] = 3500.0f;
    config->max_jerk[1] = 3500.0f;
    config->max_jerk[2] = 3500.0f;

    config->velocity_filter_alpha = 0.3f;
    config->accel_filter_alpha = 0.2f;
    config->leg_velocity_filter_alpha = 0.25f;
    config->body_velocity_filter_alpha = 0.2f;
    config->body_pitch_filter_alpha = 0.1f;
    config->body_roll_filter_alpha = 0.1f;

    config->front_to_back_distance = 0.15f;
    config->left_to_right_distance = 0.12f;
    config->max_leg_velocity = 3.0f;
    config->max_body_velocity = 1.0f;
    config->max_angular_velocity = 5.0f;
    config->min_dt = 0.001f;
    config->max_dt = 0.050f;

    config->body_offset_x = 0.0f;
    config->body_offset_y = 0.0f;
    config->body_offset_z = 0.0f;
}
