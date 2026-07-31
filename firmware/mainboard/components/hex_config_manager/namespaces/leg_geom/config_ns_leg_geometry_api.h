/*
 * Public leg geometry namespace types and APIs.
 */

#pragma once

#include "config_ns_joint_api.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float len_coxa[NUM_LEGS];
    float len_femur[NUM_LEGS];
    float len_tibia[NUM_LEGS];

    float mount_x[NUM_LEGS];
    float mount_y[NUM_LEGS];
    float mount_z[NUM_LEGS];
    float mount_yaw[NUM_LEGS];

    float stance_out[NUM_LEGS];
    float stance_fwd[NUM_LEGS];
} leg_geometry_config_t;

const leg_geometry_config_t* config_get_leg_geometry(void);
esp_err_t config_set_leg_geometry(const leg_geometry_config_t* config);

void config_load_leg_geometry_defaults(leg_geometry_config_t* config);

#ifdef __cplusplus
}
#endif
