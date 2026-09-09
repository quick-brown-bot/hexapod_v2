#include "config_domain_defaults.h"

#include <string.h>

void config_load_controller_defaults(controller_config_namespace_t* config) {
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(controller_config_namespace_t));

    config->driver_type = CONTROLLER_DRIVER_FLYSKY_IBUS;
    config->task_stack = 4096;
    config->task_priority = 10;
    config->stick_deadband = 0.04f;

    config->failsafe_vx = 0.0f;
    config->failsafe_wz = 0.0f;
    config->failsafe_z_target = 0.0f;
    config->failsafe_y_offset = 0.0f;
    config->failsafe_step_scale = 0.5f;
    config->failsafe_gait = 2; // TRIPOD
    config->failsafe_enable = false;
    config->failsafe_pose_mode = false;
    config->failsafe_terrain_climb = false;

    config->flysky_uart_port = 1;
    config->flysky_tx_gpio = -1;
    // GPIO5: where the FlySky iBUS signal wire is physically tapped on the
    // current build (board silk "D5"). Confirmed receiving valid iBUS frames
    // via firmware/mainboard_ibus_test. It is a boot-strapping pin, but iBUS
    // idles high so a powered receiver does not disturb boot. (The old value
    // 22 collided with the I2C SCL net; 34 was a mis-identification -- "34"
    // is the chip package pin number for GPIO5, not GPIO34.) A dedicated
    // 3-pin receiver header is still a pending schematic change.
    config->flysky_rx_gpio = 5;
    config->flysky_rts_gpio = -1;
    config->flysky_cts_gpio = -1;
    config->flysky_baud_rate = 115200;
}
