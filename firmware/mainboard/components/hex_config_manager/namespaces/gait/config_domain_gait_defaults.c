#include "config_domain_defaults.h"

#include <string.h>

void config_load_gait_defaults(gait_config_t* config) {
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(gait_config_t));

    // 0.5 s cycle -> at full speed phase_rate = 1/cycle_time = 2 Hz, i.e. ~2
    // steps/s per leg (tripod: each leg swings once per cycle). Was 1.5 s
    // (~0.67 steps/s), which walked at a crawl.
    config->cycle_time_s = 0.5f;
    config->step_length_m = 0.07f;
    config->clearance_height_m = 0.04f;
    config->y_range_m = 0.05f;
    config->z_min_m = -0.05f;
    config->z_max_m = -0.15f;
    config->max_yaw_per_cycle_rad = 0.4f;
    config->turn_direction = 1.0f;
}
