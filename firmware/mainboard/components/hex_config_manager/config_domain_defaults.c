#include "config_domain_defaults.h"

#include <string.h>

void config_domain_fill_joint_default(int joint, joint_calib_t* calib) {
    if (!calib || joint < 0 || joint >= NUM_JOINTS_PER_LEG) {
        return;
    }

    calib->zero_offset_rad = 0.0f;
    calib->invert_sign = (joint == 0) ? 1 : -1;
    calib->min_rad = -1.57f;
    calib->max_rad = 1.57f;
    calib->pwm_min_us = 1000;
    calib->pwm_max_us = 2000;
    calib->neutral_us = 1500;
}


