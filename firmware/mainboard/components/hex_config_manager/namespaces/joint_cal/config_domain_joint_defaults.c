#include "config_domain_defaults.h"

#include <string.h>

void config_load_joint_calib_defaults(joint_calib_config_t* config) {
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(joint_calib_config_t));

    for (int leg = 0; leg < NUM_LEGS; leg++) {
        for (int joint = 0; joint < NUM_JOINTS_PER_LEG; joint++) {
            joint_calib_t* calib = &config->joints[leg][joint];
            config_domain_fill_joint_default(joint, calib);
        }
    }
}
