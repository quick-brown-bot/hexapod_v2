#include "robot_config.h"
#include "config_manager_runtime_api.h"
#include "config_ns_joint_api.h"
#include "config_ns_leg_geometry_api.h"
#include "namespaces/servo_map/config_ns_servo_map_api.h"
#include <string.h>
#include "esp_log.h"

// Single static instance for now. In the future, load/save to NVS.
static robot_config_t g_cfg;
static float g_base_x[NUM_LEGS];
static float g_base_y[NUM_LEGS];
static float g_base_z[NUM_LEGS];
static float g_base_yaw[NUM_LEGS];
static float g_stance_out[NUM_LEGS]; // leg-local outward (+)
static float g_stance_fwd[NUM_LEGS]; // leg-local forward (+)
static const char* TAG = "robot_config";

esp_err_t robot_config_init_default(void) {
    memset(&g_cfg, 0, sizeof(g_cfg));

    // These offsets are kinematic-model constants used by leg_configure.
    // Lengths/mount/stance must come from the leg geometry namespace.
    const float coxa_offset_rad = 0.0f;
    const float femur_offset_rad = 0.5396943301595464f;
    const float tibia_offset_rad = 1.0160719600939494f;

    config_manager_state_t cfg_state = {0};

    if (config_manager_get_state(&cfg_state) != ESP_OK ||
        !cfg_state.initialized ||
        !cfg_state.namespace_loaded[CONFIG_NS_LEG_GEOMETRY] ||
        !cfg_state.namespace_loaded[CONFIG_NS_JOINT_CALIB] ||
        !cfg_state.namespace_loaded[CONFIG_NS_SERVO_MAP]) {
        ESP_LOGE(TAG, "Required configuration namespaces are not loaded");
        return ESP_ERR_INVALID_STATE;
    }

    const leg_geometry_config_t* stored_geom = config_get_leg_geometry();
    const servo_map_config_t* stored_servo_map = config_get_servo_map();
    if (!stored_geom) {
        ESP_LOGE(TAG, "Leg geometry namespace returned NULL config");
        return ESP_ERR_INVALID_STATE;
    }
    if (!stored_servo_map) {
        ESP_LOGE(TAG, "Servo map namespace returned NULL config");
        return ESP_ERR_INVALID_STATE;
    }

    for (int i = 0; i < NUM_LEGS; ++i) {
        g_cfg.mcpwm_group_id[i] = stored_servo_map->mcpwm_group_id[i];
        if (g_cfg.mcpwm_group_id[i] < 0 || g_cfg.mcpwm_group_id[i] > 1) {
            ESP_LOGE(TAG, "Invalid MCPWM group for leg %d: %d", i, g_cfg.mcpwm_group_id[i]);
            return ESP_ERR_INVALID_STATE;
        }

        for (int j = 0; j < 3; ++j) {
            g_cfg.servo_gpio[i][j] = stored_servo_map->servo_gpio[i][j];
            g_cfg.servo_driver_sel[i][j] = stored_servo_map->servo_driver_sel[i][j];

            if (g_cfg.servo_gpio[i][j] < -1 || g_cfg.servo_gpio[i][j] > 39) {
                ESP_LOGE(TAG, "Invalid servo GPIO for leg %d joint %d: %d", i, j, g_cfg.servo_gpio[i][j]);
                return ESP_ERR_INVALID_STATE;
            }
            if (g_cfg.servo_driver_sel[i][j] != 0 && g_cfg.servo_driver_sel[i][j] != 1) {
                ESP_LOGE(TAG, "Invalid servo driver selection for leg %d joint %d: %d", i, j, g_cfg.servo_driver_sel[i][j]);
                return ESP_ERR_INVALID_STATE;
            }
        }
    }

    for (int i = 0; i < NUM_LEGS; ++i) {
        leg_geometry_t leg_geom = {
            .len_coxa = stored_geom->len_coxa[i],
            .len_femur = stored_geom->len_femur[i],
            .len_tibia = stored_geom->len_tibia[i],
            .coxa_offset_rad = coxa_offset_rad,
            .femur_offset_rad = femur_offset_rad,
            .tibia_offset_rad = tibia_offset_rad,
        };
        (void)leg_configure(&leg_geom, &g_cfg.legs[i]);
    }

    // --- Joint calibration ---
    // Joint calibration is now handled directly by config_manager.
    // No caching needed - robot_config_get_joint_calib() calls config_manager on-demand.

    for (int i = 0; i < NUM_LEGS; ++i) {
        g_base_x[i] = stored_geom->mount_x[i];
        g_base_y[i] = stored_geom->mount_y[i];
        g_base_z[i] = stored_geom->mount_z[i];
        g_base_yaw[i] = stored_geom->mount_yaw[i];
        g_stance_out[i] = stored_geom->stance_out[i];
        g_stance_fwd[i] = stored_geom->stance_fwd[i];
    }

    // --- Future hardware settings (not applied here; live in robot_control) ---
    // - Servo pins and MCPWM mapping
    // - Per-joint limits/offsets/inversions
    // - Mount poses (position + yaw)

    return ESP_OK;
}

leg_handle_t robot_config_get_leg(int leg_index) {
    if (leg_index < 0 || leg_index >= NUM_LEGS) return NULL;
    return g_cfg.legs[leg_index];
}

void robot_config_get_base_pose(int leg_index, float *x, float *y, float *z, float *yaw) {
    if (x)   *x = (leg_index >= 0 && leg_index < NUM_LEGS) ? g_base_x[leg_index] : 0.0f;
    if (y)   *y = (leg_index >= 0 && leg_index < NUM_LEGS) ? g_base_y[leg_index] : 0.0f;
    if (z)   *z = (leg_index >= 0 && leg_index < NUM_LEGS) ? g_base_z[leg_index] : 0.0f;
    if (yaw) *yaw = (leg_index >= 0 && leg_index < NUM_LEGS) ? g_base_yaw[leg_index] : 0.0f;
}

const joint_calib_t* robot_config_get_joint_calib(int leg_index, leg_servo_t joint) {
    if (leg_index < 0 || leg_index >= NUM_LEGS) return NULL;
    int j = (int)joint;
    if (j < 0 || j >= 3) return NULL;
    
    // Static storage for calibration data
    static joint_calib_t temp_calib;
    
    // Get data directly from configuration manager
    if (config_get_joint_calib_data(leg_index, j, &temp_calib) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read joint calibration for leg %d joint %d", leg_index, j);
        return NULL;
    }

    return &temp_calib;
}

int robot_config_get_servo_gpio(int leg_index, leg_servo_t joint) {
    if (leg_index < 0 || leg_index >= NUM_LEGS) return -1;
    int j = (int)joint;
    if (j < 0 || j >= 3) return -1;
    return g_cfg.servo_gpio[leg_index][j];
}

int robot_config_get_mcpwm_group(int leg_index) {
    if (leg_index < 0 || leg_index >= NUM_LEGS) return 0;
    return g_cfg.mcpwm_group_id[leg_index];
}

int robot_config_get_servo_driver(int leg_index, leg_servo_t joint) {
    if (leg_index < 0 || leg_index >= NUM_LEGS) return 0;
    int j = (int)joint; if (j < 0 || j >= 3) return 0;
    return g_cfg.servo_driver_sel[leg_index][j];
}

float robot_config_get_stance_out_m(int leg_index) {
    if (leg_index < 0 || leg_index >= NUM_LEGS) return 0.0f;
    return g_stance_out[leg_index];
}
float robot_config_get_stance_fwd_m(int leg_index) {
    if (leg_index < 0 || leg_index >= NUM_LEGS) return 0.0f;
    return g_stance_fwd[leg_index];
}


