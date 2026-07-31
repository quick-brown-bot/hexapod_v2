#include <stddef.h>
#include "user_command.h"
#include "controller.h"
#include "controller_flysky_ibus.h"
#include "controller_wifi_tcp.h"
#include "controller_bt_classic.h"
#include "config_manager_runtime_api.h"
#include "namespaces/controller/config_ns_controller_api.h"
#include "esp_log.h"

static const char* TAG = "user_command";

void user_command_init(void) {
    // Start controller task with default configuration (defaults to FLYSKY iBUS driver);
    // Future: configuration portal will supply a runtime-selected driver & parameters.
    controller_init(0);

    const controller_config_t *cfg = controller_get_config();
    if (!cfg) {
        ESP_LOGE(TAG, "Controller configuration unavailable");
        return;
    }

    switch (cfg->driver_type) {
        case CONTROLLER_DRIVER_WIFI_TCP:
            controller_driver_init_wifi_tcp(cfg);
            break;
        case CONTROLLER_DRIVER_BT_CLASSIC:
            controller_driver_init_bt_classic(cfg);
            break;
        case CONTROLLER_DRIVER_FLYSKY_IBUS:
        default:
            controller_driver_init_flysky_ibus(cfg);
            break;
    }
}

void user_command_poll(user_command_t *cmd) {
    if (!cmd) return; 
    int16_t ch[CONTROLLER_MAX_CHANNELS] = {0};
    controller_state_t st = {0};
    if (controller_get_channels(ch)) {
        controller_decode(ch, &st);
        // Map to command per table
        cmd->vx = st.left_vert;   // scale to m/s elsewhere
        cmd->wz = st.left_horiz;  // map to rad/s elsewhere
        cmd->z_target = st.right_vert; // normalized -1..1, map to meters later
        cmd->y_offset = st.right_horiz;
        cmd->pose_mode = st.swb_pose;
        cmd->terrain_climb = st.swd_terrain;
        cmd->step_scale = st.sra_knob; // 0..1
        cmd->enable = st.swa_arm;
        // Gait mapping
        switch (st.swc_gait) {
            case GAIT_MODE_WAVE:   cmd->gait = GAIT_WAVE; break;
            case GAIT_MODE_RIPPLE: cmd->gait = GAIT_RIPPLE; break;
            case GAIT_MODE_TRIPOD: default: cmd->gait = GAIT_TRIPOD; break;
        }
    } else {                   
        // Use configured controller failsafe profile when no data is available.
        const controller_config_namespace_t *cfg = config_get_controller();
        if (cfg) {
            cmd->vx = cfg->failsafe_vx;
            cmd->wz = cfg->failsafe_wz;
            cmd->z_target = cfg->failsafe_z_target;
            cmd->y_offset = cfg->failsafe_y_offset;
            cmd->enable = cfg->failsafe_enable;
            cmd->pose_mode = cfg->failsafe_pose_mode;
            cmd->terrain_climb = cfg->failsafe_terrain_climb;
            cmd->step_scale = cfg->failsafe_step_scale;

            switch (cfg->failsafe_gait) {
                case 0: cmd->gait = GAIT_WAVE; break;
                case 1: cmd->gait = GAIT_RIPPLE; break;
                case 2:
                default:
                    cmd->gait = GAIT_TRIPOD;
                    break;
            }
        } else {
            cmd->vx = 0.0f;
            cmd->wz = 0.0f;
            cmd->z_target = 0.0f;
            cmd->y_offset = 0.0f;
            cmd->gait = GAIT_TRIPOD;
            cmd->enable = false;
            cmd->pose_mode = false;
            cmd->terrain_climb = false;
            cmd->step_scale = 0.5f;
        }

        ESP_LOGI(TAG, "No controller data available, using failsafe profile");
    }
}