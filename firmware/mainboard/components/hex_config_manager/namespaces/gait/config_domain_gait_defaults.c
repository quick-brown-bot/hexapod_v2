#include "config_domain_defaults.h"

#include <string.h>

void config_load_gait_defaults(gait_config_t* config) {
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(gait_config_t));

    config->cycle_time_s = 1.5f;
    config->step_length_m = 0.07f;
    config->clearance_height_m = 0.04f;
    config->y_range_m = 0.05f;
    config->z_min_m = -0.05f;
    config->z_max_m = -0.15f;
    config->max_yaw_per_cycle_rad = 0.4f;
    config->turn_direction = 1.0f;
}
