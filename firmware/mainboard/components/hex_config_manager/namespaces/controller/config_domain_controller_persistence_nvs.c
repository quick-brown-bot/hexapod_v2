#include "config_domain_persistence_nvs.h"

#include "config_domain_defaults.h"
#include "esp_log.h"

static const char *TAG = "cfg_ctrl_nvs";

#define CTRL_KEY_DRIVER_TYPE     "drv_t"
#define CTRL_KEY_TASK_STACK      "tsk_stk"
#define CTRL_KEY_TASK_PRIORITY   "tsk_pri"
#define CTRL_KEY_STICK_DEADBAND  "stick_db"
#define CTRL_KEY_FAILSAFE_VX     "fs_vx"
#define CTRL_KEY_FAILSAFE_WZ     "fs_wz"
#define CTRL_KEY_FAILSAFE_Z      "fs_z"
#define CTRL_KEY_FAILSAFE_Y      "fs_y"
#define CTRL_KEY_FAILSAFE_STEP   "fs_step"
#define CTRL_KEY_FAILSAFE_GAIT   "fs_gait"
#define CTRL_KEY_FAILSAFE_EN     "fs_en"
#define CTRL_KEY_FAILSAFE_POSE   "fs_pose"
#define CTRL_KEY_FAILSAFE_TERR   "fs_terr"
#define CTRL_KEY_FLYSKY_UART     "fly_u"
#define CTRL_KEY_FLYSKY_TX       "fly_tx"
#define CTRL_KEY_FLYSKY_RX       "fly_rx"
#define CTRL_KEY_FLYSKY_RTS      "fly_rts"
#define CTRL_KEY_FLYSKY_CTS      "fly_cts"
#define CTRL_KEY_FLYSKY_BAUD     "fly_baud"

static esp_err_t nvs_set_float_blob(nvs_handle_t handle, const char* key, float value) {
    return nvs_set_blob(handle, key, &value, sizeof(float));
}

static esp_err_t nvs_get_float_blob(nvs_handle_t handle, const char* key, float* value_out) {
    size_t required_size = sizeof(float);
    return nvs_get_blob(handle, key, value_out, &required_size);
}

esp_err_t config_domain_controller_write_defaults_to_nvs(nvs_handle_t handle) {
    controller_config_namespace_t defaults;

    config_load_controller_defaults(&defaults);

    ESP_LOGI(TAG, "Initializing controller namespace defaults to NVS");

    ESP_ERROR_CHECK(nvs_set_u8(handle, CTRL_KEY_DRIVER_TYPE, (uint8_t)defaults.driver_type));
    ESP_ERROR_CHECK(nvs_set_u32(handle, CTRL_KEY_TASK_STACK, defaults.task_stack));
    ESP_ERROR_CHECK(nvs_set_u32(handle, CTRL_KEY_TASK_PRIORITY, defaults.task_priority));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, CTRL_KEY_STICK_DEADBAND, defaults.stick_deadband));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, CTRL_KEY_FAILSAFE_VX, defaults.failsafe_vx));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, CTRL_KEY_FAILSAFE_WZ, defaults.failsafe_wz));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, CTRL_KEY_FAILSAFE_Z, defaults.failsafe_z_target));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, CTRL_KEY_FAILSAFE_Y, defaults.failsafe_y_offset));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, CTRL_KEY_FAILSAFE_STEP, defaults.failsafe_step_scale));
    ESP_ERROR_CHECK(nvs_set_i32(handle, CTRL_KEY_FAILSAFE_GAIT, defaults.failsafe_gait));
    ESP_ERROR_CHECK(nvs_set_u8(handle, CTRL_KEY_FAILSAFE_EN, defaults.failsafe_enable ? 1 : 0));
    ESP_ERROR_CHECK(nvs_set_u8(handle, CTRL_KEY_FAILSAFE_POSE, defaults.failsafe_pose_mode ? 1 : 0));
    ESP_ERROR_CHECK(nvs_set_u8(handle, CTRL_KEY_FAILSAFE_TERR, defaults.failsafe_terrain_climb ? 1 : 0));

    ESP_ERROR_CHECK(nvs_set_i32(handle, CTRL_KEY_FLYSKY_UART, defaults.flysky_uart_port));
    ESP_ERROR_CHECK(nvs_set_i32(handle, CTRL_KEY_FLYSKY_TX, defaults.flysky_tx_gpio));
    ESP_ERROR_CHECK(nvs_set_i32(handle, CTRL_KEY_FLYSKY_RX, defaults.flysky_rx_gpio));
    ESP_ERROR_CHECK(nvs_set_i32(handle, CTRL_KEY_FLYSKY_RTS, defaults.flysky_rts_gpio));
    ESP_ERROR_CHECK(nvs_set_i32(handle, CTRL_KEY_FLYSKY_CTS, defaults.flysky_cts_gpio));
    ESP_ERROR_CHECK(nvs_set_i32(handle, CTRL_KEY_FLYSKY_BAUD, defaults.flysky_baud_rate));

    ESP_ERROR_CHECK(nvs_commit(handle));
    ESP_LOGI(TAG, "Controller defaults written to NVS");

    return ESP_OK;
}

esp_err_t config_domain_controller_load_from_nvs(
    nvs_handle_t handle,
    controller_config_namespace_t* config
) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err;
    uint8_t driver_type = 0;

    ESP_LOGI(TAG, "Loading controller configuration from NVS (read-only)");

    err = nvs_get_u8(handle, CTRL_KEY_DRIVER_TYPE, &driver_type);
    if (err == ESP_OK) {
        config->driver_type = (controller_driver_type_e)driver_type;
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load driver_type: %s", esp_err_to_name(err));
    }

    err = nvs_get_u32(handle, CTRL_KEY_TASK_STACK, &config->task_stack);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load task_stack: %s", esp_err_to_name(err));
    }

    err = nvs_get_u32(handle, CTRL_KEY_TASK_PRIORITY, &config->task_priority);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load task_priority: %s", esp_err_to_name(err));
    }

    err = nvs_get_float_blob(handle, CTRL_KEY_STICK_DEADBAND, &config->stick_deadband);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load stick_deadband: %s", esp_err_to_name(err));
    }

    err = nvs_get_float_blob(handle, CTRL_KEY_FAILSAFE_VX, &config->failsafe_vx);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load failsafe_vx: %s", esp_err_to_name(err));
    }

    err = nvs_get_float_blob(handle, CTRL_KEY_FAILSAFE_WZ, &config->failsafe_wz);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load failsafe_wz: %s", esp_err_to_name(err));
    }

    err = nvs_get_float_blob(handle, CTRL_KEY_FAILSAFE_Z, &config->failsafe_z_target);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load failsafe_z_target: %s", esp_err_to_name(err));
    }

    err = nvs_get_float_blob(handle, CTRL_KEY_FAILSAFE_Y, &config->failsafe_y_offset);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load failsafe_y_offset: %s", esp_err_to_name(err));
    }

    err = nvs_get_float_blob(handle, CTRL_KEY_FAILSAFE_STEP, &config->failsafe_step_scale);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load failsafe_step_scale: %s", esp_err_to_name(err));
    }

    err = nvs_get_i32(handle, CTRL_KEY_FAILSAFE_GAIT, &config->failsafe_gait);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load failsafe_gait: %s", esp_err_to_name(err));
    }

    uint8_t failsafe_enable = 0;
    err = nvs_get_u8(handle, CTRL_KEY_FAILSAFE_EN, &failsafe_enable);
    if (err == ESP_OK) {
        config->failsafe_enable = (failsafe_enable != 0);
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load failsafe_enable: %s", esp_err_to_name(err));
    }

    uint8_t failsafe_pose_mode = 0;
    err = nvs_get_u8(handle, CTRL_KEY_FAILSAFE_POSE, &failsafe_pose_mode);
    if (err == ESP_OK) {
        config->failsafe_pose_mode = (failsafe_pose_mode != 0);
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load failsafe_pose_mode: %s", esp_err_to_name(err));
    }

    uint8_t failsafe_terrain = 0;
    err = nvs_get_u8(handle, CTRL_KEY_FAILSAFE_TERR, &failsafe_terrain);
    if (err == ESP_OK) {
        config->failsafe_terrain_climb = (failsafe_terrain != 0);
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load failsafe_terrain_climb: %s", esp_err_to_name(err));
    }

    err = nvs_get_i32(handle, CTRL_KEY_FLYSKY_UART, &config->flysky_uart_port);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load flysky_uart_port: %s", esp_err_to_name(err));
    }

    err = nvs_get_i32(handle, CTRL_KEY_FLYSKY_TX, &config->flysky_tx_gpio);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load flysky_tx_gpio: %s", esp_err_to_name(err));
    }

    err = nvs_get_i32(handle, CTRL_KEY_FLYSKY_RX, &config->flysky_rx_gpio);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load flysky_rx_gpio: %s", esp_err_to_name(err));
    }

    err = nvs_get_i32(handle, CTRL_KEY_FLYSKY_RTS, &config->flysky_rts_gpio);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load flysky_rts_gpio: %s", esp_err_to_name(err));
    }

    err = nvs_get_i32(handle, CTRL_KEY_FLYSKY_CTS, &config->flysky_cts_gpio);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load flysky_cts_gpio: %s", esp_err_to_name(err));
    }

    err = nvs_get_i32(handle, CTRL_KEY_FLYSKY_BAUD, &config->flysky_baud_rate);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load flysky_baud_rate: %s", esp_err_to_name(err));
    }

    return ESP_OK;
}

esp_err_t config_domain_controller_save_to_nvs(
    nvs_handle_t handle,
    const controller_config_namespace_t* config
) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_ERROR_CHECK(nvs_set_u8(handle, CTRL_KEY_DRIVER_TYPE, (uint8_t)config->driver_type));
    ESP_ERROR_CHECK(nvs_set_u32(handle, CTRL_KEY_TASK_STACK, config->task_stack));
    ESP_ERROR_CHECK(nvs_set_u32(handle, CTRL_KEY_TASK_PRIORITY, config->task_priority));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, CTRL_KEY_STICK_DEADBAND, config->stick_deadband));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, CTRL_KEY_FAILSAFE_VX, config->failsafe_vx));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, CTRL_KEY_FAILSAFE_WZ, config->failsafe_wz));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, CTRL_KEY_FAILSAFE_Z, config->failsafe_z_target));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, CTRL_KEY_FAILSAFE_Y, config->failsafe_y_offset));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, CTRL_KEY_FAILSAFE_STEP, config->failsafe_step_scale));
    ESP_ERROR_CHECK(nvs_set_i32(handle, CTRL_KEY_FAILSAFE_GAIT, config->failsafe_gait));
    ESP_ERROR_CHECK(nvs_set_u8(handle, CTRL_KEY_FAILSAFE_EN, config->failsafe_enable ? 1 : 0));
    ESP_ERROR_CHECK(nvs_set_u8(handle, CTRL_KEY_FAILSAFE_POSE, config->failsafe_pose_mode ? 1 : 0));
    ESP_ERROR_CHECK(nvs_set_u8(handle, CTRL_KEY_FAILSAFE_TERR, config->failsafe_terrain_climb ? 1 : 0));

    ESP_ERROR_CHECK(nvs_set_i32(handle, CTRL_KEY_FLYSKY_UART, config->flysky_uart_port));
    ESP_ERROR_CHECK(nvs_set_i32(handle, CTRL_KEY_FLYSKY_TX, config->flysky_tx_gpio));
    ESP_ERROR_CHECK(nvs_set_i32(handle, CTRL_KEY_FLYSKY_RX, config->flysky_rx_gpio));
    ESP_ERROR_CHECK(nvs_set_i32(handle, CTRL_KEY_FLYSKY_RTS, config->flysky_rts_gpio));
    ESP_ERROR_CHECK(nvs_set_i32(handle, CTRL_KEY_FLYSKY_CTS, config->flysky_cts_gpio));
    ESP_ERROR_CHECK(nvs_set_i32(handle, CTRL_KEY_FLYSKY_BAUD, config->flysky_baud_rate));

    return nvs_commit(handle);
}
