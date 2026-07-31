#include "config_domain_defaults.h"

#include <math.h>
#include <string.h>

void config_load_leg_geometry_defaults(leg_geometry_config_t* config) {
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(leg_geometry_config_t));

    const float default_len_coxa = 0.068f;
    const float default_len_femur = 0.088f;
    const float default_len_tibia = 0.127f;

    const float x_off_front = 0.08f;
    const float x_off_rear = -0.08f;
    const float y_off_left = 0.05f;
    const float y_off_right = -0.05f;
    const float z_off = 0.0f;
    const float yaw_left = (float)M_PI * 0.5f;
    const float yaw_right = (float)-M_PI * 0.5f;
    const float qangle = (float)M_PI * 0.25f;

    const float default_stance_out = 0.15f;
    const float default_stance_fwd = 0.0f;

    for (int leg = 0; leg < NUM_LEGS; leg++) {
        config->len_coxa[leg] = default_len_coxa;
        config->len_femur[leg] = default_len_femur;
        config->len_tibia[leg] = default_len_tibia;

        config->mount_z[leg] = z_off;
        config->stance_out[leg] = default_stance_out;
        config->stance_fwd[leg] = default_stance_fwd;
    }

    config->mount_x[0] = x_off_front;
    config->mount_y[0] = y_off_left;
    config->mount_yaw[0] = yaw_left - qangle;

    config->mount_x[1] = 0.0f;
    config->mount_y[1] = y_off_left;
    config->mount_yaw[1] = yaw_left;

    config->mount_x[2] = x_off_rear;
    config->mount_y[2] = y_off_left;
    config->mount_yaw[2] = yaw_left + qangle;

    config->mount_x[3] = x_off_front;
    config->mount_y[3] = y_off_right;
    config->mount_yaw[3] = yaw_right + qangle;

    config->mount_x[4] = 0.0f;
    config->mount_y[4] = y_off_right;
    config->mount_yaw[4] = yaw_right;

    config->mount_x[5] = x_off_rear;
    config->mount_y[5] = y_off_right;
    config->mount_yaw[5] = yaw_right - qangle;
}
