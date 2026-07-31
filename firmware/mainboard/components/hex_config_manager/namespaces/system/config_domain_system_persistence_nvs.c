#include "config_domain_persistence_nvs.h"

#include "esp_log.h"
#include "esp_mac.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "cfg_sys_nvs";

#define SYS_KEY_EMERGENCY_STOP      "emerg_stop"
#define SYS_KEY_AUTO_DISARM_TIMEOUT "auto_disarm"
#define SYS_KEY_SAFETY_VOLTAGE_MIN  "volt_min"
#define SYS_KEY_TEMP_LIMIT_MAX      "temp_max"
#define SYS_KEY_MOTION_TIMEOUT      "motion_timeout"
#define SYS_KEY_STARTUP_DELAY       "startup_delay"
#define SYS_KEY_MAX_CONTROL_FREQ    "max_ctrl_freq"
#define SYS_KEY_ROBOT_ID            "robot_id"
#define SYS_KEY_ROBOT_NAME          "robot_name"
#define SYS_KEY_CONFIG_VERSION      "config_ver"
#define SYS_KEY_CONTROLLER_TYPE     "ctrl_type"

esp_err_t config_domain_system_write_defaults_to_nvs(
    nvs_handle_t handle,
    uint16_t schema_version
) {
    ESP_LOGI(TAG, "Initializing system namespace defaults to NVS");

    ESP_ERROR_CHECK(nvs_set_u8(handle, SYS_KEY_EMERGENCY_STOP, 1));
    ESP_ERROR_CHECK(nvs_set_u32(handle, SYS_KEY_AUTO_DISARM_TIMEOUT, 30));
    ESP_ERROR_CHECK(nvs_set_u32(handle, SYS_KEY_MOTION_TIMEOUT, 1000));
    ESP_ERROR_CHECK(nvs_set_u32(handle, SYS_KEY_STARTUP_DELAY, 2000));
    ESP_ERROR_CHECK(nvs_set_u32(handle, SYS_KEY_MAX_CONTROL_FREQ, 100));

    float default_voltage = 6.5f;
    float default_temp = 80.0f;
    ESP_ERROR_CHECK(nvs_set_blob(handle, SYS_KEY_SAFETY_VOLTAGE_MIN, &default_voltage, sizeof(float)));
    ESP_ERROR_CHECK(nvs_set_blob(handle, SYS_KEY_TEMP_LIMIT_MAX, &default_temp, sizeof(float)));

    char robot_id[32];
    uint8_t mac[6];
    esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (err == ESP_OK) {
        snprintf(robot_id, sizeof(robot_id), "HEXAPOD_%02X%02X%02X", mac[3], mac[4], mac[5]);
    } else {
        strcpy(robot_id, "HEXAPOD_DEFAULT");
    }

    ESP_ERROR_CHECK(nvs_set_str(handle, SYS_KEY_ROBOT_ID, robot_id));
    ESP_ERROR_CHECK(nvs_set_str(handle, SYS_KEY_ROBOT_NAME, "My Hexapod Robot"));
    ESP_ERROR_CHECK(nvs_set_u16(handle, SYS_KEY_CONFIG_VERSION, schema_version));
    ESP_ERROR_CHECK(nvs_set_u8(handle, SYS_KEY_CONTROLLER_TYPE, (uint8_t)CONTROLLER_DRIVER_FLYSKY_IBUS));

    ESP_ERROR_CHECK(nvs_commit(handle));
    ESP_LOGI(TAG, "System defaults written to NVS");

    return ESP_OK;
}

esp_err_t config_domain_system_load_from_nvs(
    nvs_handle_t handle,
    system_config_t* config,
    uint16_t fallback_schema_version,
    controller_driver_type_e fallback_controller_type
) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err;
    size_t required_size = 0;

    ESP_LOGI(TAG, "Loading system configuration from NVS (read-only)");

    uint8_t temp_bool = 0;
    err = nvs_get_u8(handle, SYS_KEY_EMERGENCY_STOP, &temp_bool);
    if (err == ESP_OK) {
        config->emergency_stop_enabled = (temp_bool != 0);
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to read emergency_stop, using default: %s", esp_err_to_name(err));
    }

    err = nvs_get_u32(handle, SYS_KEY_AUTO_DISARM_TIMEOUT, &config->auto_disarm_timeout);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to read auto_disarm_timeout: %s", esp_err_to_name(err));
    }

    err = nvs_get_u32(handle, SYS_KEY_MOTION_TIMEOUT, &config->motion_timeout_ms);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to read motion_timeout: %s", esp_err_to_name(err));
    }

    err = nvs_get_u32(handle, SYS_KEY_STARTUP_DELAY, &config->startup_delay_ms);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to read startup_delay: %s", esp_err_to_name(err));
    }

    err = nvs_get_u32(handle, SYS_KEY_MAX_CONTROL_FREQ, &config->max_control_frequency);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to read max_control_freq: %s", esp_err_to_name(err));
    }

    required_size = sizeof(float);
    err = nvs_get_blob(handle, SYS_KEY_SAFETY_VOLTAGE_MIN, &config->safety_voltage_min, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to read safety_voltage_min: %s", esp_err_to_name(err));
    }

    required_size = sizeof(float);
    err = nvs_get_blob(handle, SYS_KEY_TEMP_LIMIT_MAX, &config->temperature_limit_max, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to read temp_limit_max: %s", esp_err_to_name(err));
    }

    required_size = sizeof(config->robot_id);
    err = nvs_get_str(handle, SYS_KEY_ROBOT_ID, config->robot_id, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to read robot_id: %s", esp_err_to_name(err));
    }

    required_size = sizeof(config->robot_name);
    err = nvs_get_str(handle, SYS_KEY_ROBOT_NAME, config->robot_name, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to read robot_name: %s", esp_err_to_name(err));
    }

    uint16_t namespace_version = 0;
    err = nvs_get_u16(handle, SYS_KEY_CONFIG_VERSION, &namespace_version);
    if (err == ESP_OK) {
        config->config_version = namespace_version;
    } else {
        ESP_LOGW(TAG, "Failed to read namespace config version: %s", esp_err_to_name(err));
        config->config_version = fallback_schema_version;
    }

    uint8_t ctrl_type = 0;
    err = nvs_get_u8(handle, SYS_KEY_CONTROLLER_TYPE, &ctrl_type);
    if (err == ESP_OK) {
        config->controller_type = (controller_driver_type_e)ctrl_type;
    } else {
        ESP_LOGW(TAG, "Failed to read controller_type, using default: %s", esp_err_to_name(err));
        config->controller_type = fallback_controller_type;
    }

    return ESP_OK;
}

esp_err_t config_domain_system_save_to_nvs(
    nvs_handle_t handle,
    const system_config_t* config
) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t temp_bool = config->emergency_stop_enabled ? 1 : 0;
    ESP_ERROR_CHECK(nvs_set_u8(handle, SYS_KEY_EMERGENCY_STOP, temp_bool));
    ESP_ERROR_CHECK(nvs_set_u32(handle, SYS_KEY_AUTO_DISARM_TIMEOUT, config->auto_disarm_timeout));
    ESP_ERROR_CHECK(nvs_set_u32(handle, SYS_KEY_MOTION_TIMEOUT, config->motion_timeout_ms));
    ESP_ERROR_CHECK(nvs_set_u32(handle, SYS_KEY_STARTUP_DELAY, config->startup_delay_ms));
    ESP_ERROR_CHECK(nvs_set_u32(handle, SYS_KEY_MAX_CONTROL_FREQ, config->max_control_frequency));

    ESP_ERROR_CHECK(nvs_set_blob(handle, SYS_KEY_SAFETY_VOLTAGE_MIN, &config->safety_voltage_min, sizeof(float)));
    ESP_ERROR_CHECK(nvs_set_blob(handle, SYS_KEY_TEMP_LIMIT_MAX, &config->temperature_limit_max, sizeof(float)));

    ESP_ERROR_CHECK(nvs_set_str(handle, SYS_KEY_ROBOT_ID, config->robot_id));
    ESP_ERROR_CHECK(nvs_set_str(handle, SYS_KEY_ROBOT_NAME, config->robot_name));

    ESP_ERROR_CHECK(nvs_set_u16(handle, SYS_KEY_CONFIG_VERSION, config->config_version));
    ESP_ERROR_CHECK(nvs_set_u8(handle, SYS_KEY_CONTROLLER_TYPE, (uint8_t)config->controller_type));

    return nvs_commit(handle);
}
