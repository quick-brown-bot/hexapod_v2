#include "config_domain_defaults.h"

#include <string.h>

void config_load_servo_map_defaults(servo_map_config_t* config) {
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(servo_map_config_t));

    for (int leg = 0; leg < NUM_LEGS; leg++) {
        config->mcpwm_group_id[leg] = (leg < 3) ? 0 : 1;
        for (int joint = 0; joint < NUM_JOINTS_PER_LEG; joint++) {
            config->servo_gpio[leg][joint] = -1;
            config->servo_driver_sel[leg][joint] = 0; // MCPWM
        }
    }

    config->mcpwm_group_id[2] = 1;

    config->servo_gpio[0][0] = 27;
    config->servo_gpio[0][1] = 13;
    config->servo_gpio[0][2] = 12;

    config->servo_gpio[1][0] = 14;
    config->servo_gpio[1][1] = 26;
    config->servo_gpio[1][2] = 25;

    config->servo_gpio[2][0] = 23;
    config->servo_gpio[2][1] = 32;
    config->servo_gpio[2][2] = 33;

    config->servo_gpio[3][0] = 5;
    config->servo_gpio[3][1] = 17;
    config->servo_gpio[3][2] = 16;

    config->servo_gpio[4][0] = 4;
    config->servo_gpio[4][1] = 2;
    config->servo_gpio[4][2] = 15;

    config->servo_gpio[5][0] = 21;
    config->servo_gpio[5][1] = 19;
    config->servo_gpio[5][2] = 18;

    for (int joint = 0; joint < NUM_JOINTS_PER_LEG; joint++) {
        config->servo_driver_sel[4][joint] = 1; // LEDC
        config->servo_driver_sel[5][joint] = 1; // LEDC
    }
}
