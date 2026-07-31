#include "config_domain_persistence_nvs.h"

#include "config_domain_defaults.h"
#include "esp_log.h"

#include <stdio.h>

static const char *TAG = "cfg_smap_nvs";

#define SMAP_KEY_GROUP_FORMAT  "l%d_grp"
#define SMAP_KEY_GPIO_FORMAT   "l%d_%sg"
#define SMAP_KEY_DRV_FORMAT    "l%d_%sd"

static const char* JOINT_NAMES[] = { "c", "f", "t" };

esp_err_t config_domain_servo_map_write_defaults_to_nvs(nvs_handle_t handle) {
    servo_map_config_t defaults;
    char key[16];

    config_load_servo_map_defaults(&defaults);

    ESP_LOGI(TAG, "Initializing servo map namespace defaults to NVS");

    for (int leg = 0; leg < NUM_LEGS; leg++) {
        snprintf(key, sizeof(key), SMAP_KEY_GROUP_FORMAT, leg);
        ESP_ERROR_CHECK(nvs_set_i32(handle, key, defaults.mcpwm_group_id[leg]));

        for (int joint = 0; joint < NUM_JOINTS_PER_LEG; joint++) {
            const char* joint_name = JOINT_NAMES[joint];

            snprintf(key, sizeof(key), SMAP_KEY_GPIO_FORMAT, leg, joint_name);
            ESP_ERROR_CHECK(nvs_set_i32(handle, key, defaults.servo_gpio[leg][joint]));

            snprintf(key, sizeof(key), SMAP_KEY_DRV_FORMAT, leg, joint_name);
            ESP_ERROR_CHECK(nvs_set_i32(handle, key, defaults.servo_driver_sel[leg][joint]));
        }
    }

    ESP_ERROR_CHECK(nvs_commit(handle));
    ESP_LOGI(TAG, "Servo map defaults written to NVS");

    return ESP_OK;
}

esp_err_t config_domain_servo_map_load_from_nvs(
    nvs_handle_t handle,
    servo_map_config_t* config
) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err;
    char key[16];

    ESP_LOGI(TAG, "Loading servo map configuration from NVS (read-only)");

    for (int leg = 0; leg < NUM_LEGS; leg++) {
        snprintf(key, sizeof(key), SMAP_KEY_GROUP_FORMAT, leg);
        err = nvs_get_i32(handle, key, &config->mcpwm_group_id[leg]);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Failed to load group for leg %d: %s", leg, esp_err_to_name(err));
        }

        for (int joint = 0; joint < NUM_JOINTS_PER_LEG; joint++) {
            const char* joint_name = JOINT_NAMES[joint];

            snprintf(key, sizeof(key), SMAP_KEY_GPIO_FORMAT, leg, joint_name);
            err = nvs_get_i32(handle, key, &config->servo_gpio[leg][joint]);
            if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
                ESP_LOGW(TAG, "Failed to load gpio for leg %d joint %s: %s", leg, joint_name, esp_err_to_name(err));
            }

            snprintf(key, sizeof(key), SMAP_KEY_DRV_FORMAT, leg, joint_name);
            err = nvs_get_i32(handle, key, &config->servo_driver_sel[leg][joint]);
            if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
                ESP_LOGW(TAG, "Failed to load driver for leg %d joint %s: %s", leg, joint_name, esp_err_to_name(err));
            }
        }
    }

    return ESP_OK;
}

esp_err_t config_domain_servo_map_save_to_nvs(
    nvs_handle_t handle,
    const servo_map_config_t* config
) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    char key[16];

    for (int leg = 0; leg < NUM_LEGS; leg++) {
        snprintf(key, sizeof(key), SMAP_KEY_GROUP_FORMAT, leg);
        ESP_ERROR_CHECK(nvs_set_i32(handle, key, config->mcpwm_group_id[leg]));

        for (int joint = 0; joint < NUM_JOINTS_PER_LEG; joint++) {
            const char* joint_name = JOINT_NAMES[joint];

            snprintf(key, sizeof(key), SMAP_KEY_GPIO_FORMAT, leg, joint_name);
            ESP_ERROR_CHECK(nvs_set_i32(handle, key, config->servo_gpio[leg][joint]));

            snprintf(key, sizeof(key), SMAP_KEY_DRV_FORMAT, leg, joint_name);
            ESP_ERROR_CHECK(nvs_set_i32(handle, key, config->servo_driver_sel[leg][joint]));
        }
    }

    return nvs_commit(handle);
}
