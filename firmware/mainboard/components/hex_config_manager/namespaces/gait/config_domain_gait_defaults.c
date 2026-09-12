#include "config_domain_defaults.h"

#include <string.h>

void config_load_gait_defaults(gait_config_t* config) {
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(gait_config_t));

    // 0.25 s cycle -> at full speed phase_rate = 1/cycle_time = 4 Hz, i.e. ~4
    // steps/s per leg (tripod: each leg swings once per cycle). Was 0.5 s
    // (~2 steps/s).
    config->cycle_time_s = 0.25f;
    config->step_length_m = 0.09f;
    config->clearance_height_m = 0.04f;
    config->y_range_m = 0.05f;
    config->z_min_m = -0.05f;
    config->z_max_m = -0.15f;
    config->max_yaw_per_cycle_rad = 0.4f;
    config->turn_direction = 1.0f;
    // Wilson gait continuum bounds; see config_ns_gait_api.h. Matches the
    // old fixed wave (S~0.1) and tripod (S=0.5) duty factors as the
    // continuum's endpoints.
    config->duty_factor_min = 0.1f;
    config->duty_factor_max = 0.5f;
}
