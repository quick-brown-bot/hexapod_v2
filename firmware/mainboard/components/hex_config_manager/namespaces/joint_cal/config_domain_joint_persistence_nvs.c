#include "config_domain_persistence_nvs.h"

#include "config_domain_defaults.h"
#include "esp_log.h"

#include <stdio.h>

static const char *TAG = "cfg_joint_nvs";

#define JOINT_KEY_OFFSET_FORMAT  "l%d_%s_off"
#define JOINT_KEY_INVERT_FORMAT  "l%d_%s_inv"
#define JOINT_KEY_MIN_FORMAT     "l%d_%s_min"
#define JOINT_KEY_MAX_FORMAT     "l%d_%s_max"
#define JOINT_KEY_PWM_MIN_FORMAT "l%d_%s_pmin"
#define JOINT_KEY_PWM_MAX_FORMAT "l%d_%s_pmax"
#define JOINT_KEY_NEUTRAL_FORMAT "l%d_%s_neut"

static const char* JOINT_NAMES[] = { "c", "f", "t" };

esp_err_t config_domain_joint_cal_write_defaults_to_nvs(nvs_handle_t handle) {
    char key[32];

    ESP_LOGI(TAG, "Initializing joint calibration namespace defaults to NVS");

    for (int leg = 0; leg < NUM_LEGS; leg++) {
        for (int joint = 0; joint < NUM_JOINTS_PER_LEG; joint++) {
            const char* joint_name = JOINT_NAMES[joint];

            joint_calib_t default_calib;
            config_domain_fill_joint_default(joint, &default_calib);

            snprintf(key, sizeof(key), JOINT_KEY_OFFSET_FORMAT, leg, joint_name);
            ESP_ERROR_CHECK(nvs_set_blob(handle, key, &default_calib.zero_offset_rad, sizeof(float)));

            snprintf(key, sizeof(key), JOINT_KEY_INVERT_FORMAT, leg, joint_name);
            ESP_ERROR_CHECK(nvs_set_i8(handle, key, default_calib.invert_sign));

            snprintf(key, sizeof(key), JOINT_KEY_MIN_FORMAT, leg, joint_name);
            ESP_ERROR_CHECK(nvs_set_blob(handle, key, &default_calib.min_rad, sizeof(float)));

            snprintf(key, sizeof(key), JOINT_KEY_MAX_FORMAT, leg, joint_name);
            ESP_ERROR_CHECK(nvs_set_blob(handle, key, &default_calib.max_rad, sizeof(float)));

            snprintf(key, sizeof(key), JOINT_KEY_PWM_MIN_FORMAT, leg, joint_name);
            ESP_ERROR_CHECK(nvs_set_i32(handle, key, default_calib.pwm_min_us));

            snprintf(key, sizeof(key), JOINT_KEY_PWM_MAX_FORMAT, leg, joint_name);
            ESP_ERROR_CHECK(nvs_set_i32(handle, key, default_calib.pwm_max_us));

            snprintf(key, sizeof(key), JOINT_KEY_NEUTRAL_FORMAT, leg, joint_name);
            ESP_ERROR_CHECK(nvs_set_i32(handle, key, default_calib.neutral_us));
        }
    }

    ESP_ERROR_CHECK(nvs_commit(handle));
    ESP_LOGI(TAG, "Joint calibration defaults written to NVS");

    return ESP_OK;
}

esp_err_t config_domain_joint_cal_load_from_nvs(
    nvs_handle_t handle,
    joint_calib_config_t* config
) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err;
    char key[32];

    ESP_LOGI(TAG, "Loading joint calibration configuration from NVS (read-only)");

    for (int leg = 0; leg < NUM_LEGS; leg++) {
        for (int joint = 0; joint < NUM_JOINTS_PER_LEG; joint++) {
            const char* joint_name = JOINT_NAMES[joint];
            joint_calib_t* calib = &config->joints[leg][joint];

            snprintf(key, sizeof(key), JOINT_KEY_OFFSET_FORMAT, leg, joint_name);
            size_t required_size = sizeof(float);
            err = nvs_get_blob(handle, key, &calib->zero_offset_rad, &required_size);
            if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
                ESP_LOGW(TAG, "Failed to load offset for leg %d joint %s: %s", leg, joint_name, esp_err_to_name(err));
            }

            snprintf(key, sizeof(key), JOINT_KEY_INVERT_FORMAT, leg, joint_name);
            err = nvs_get_i8(handle, key, &calib->invert_sign);
            if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
                ESP_LOGW(TAG, "Failed to load invert for leg %d joint %s: %s", leg, joint_name, esp_err_to_name(err));
            }

            snprintf(key, sizeof(key), JOINT_KEY_MIN_FORMAT, leg, joint_name);
            required_size = sizeof(float);
            err = nvs_get_blob(handle, key, &calib->min_rad, &required_size);
            if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
                ESP_LOGW(TAG, "Failed to load min limit for leg %d joint %s: %s", leg, joint_name, esp_err_to_name(err));
            }

            snprintf(key, sizeof(key), JOINT_KEY_MAX_FORMAT, leg, joint_name);
            required_size = sizeof(float);
            err = nvs_get_blob(handle, key, &calib->max_rad, &required_size);
            if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
                ESP_LOGW(TAG, "Failed to load max limit for leg %d joint %s: %s", leg, joint_name, esp_err_to_name(err));
            }

            snprintf(key, sizeof(key), JOINT_KEY_PWM_MIN_FORMAT, leg, joint_name);
            err = nvs_get_i32(handle, key, &calib->pwm_min_us);
            if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
                ESP_LOGW(TAG, "Failed to load PWM min for leg %d joint %s: %s", leg, joint_name, esp_err_to_name(err));
            }

            snprintf(key, sizeof(key), JOINT_KEY_PWM_MAX_FORMAT, leg, joint_name);
            err = nvs_get_i32(handle, key, &calib->pwm_max_us);
            if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
                ESP_LOGW(TAG, "Failed to load PWM max for leg %d joint %s: %s", leg, joint_name, esp_err_to_name(err));
            }

            snprintf(key, sizeof(key), JOINT_KEY_NEUTRAL_FORMAT, leg, joint_name);
            err = nvs_get_i32(handle, key, &calib->neutral_us);
            if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
                ESP_LOGW(TAG, "Failed to load PWM neutral for leg %d joint %s: %s", leg, joint_name, esp_err_to_name(err));
            }
        }
    }

    return ESP_OK;
}

esp_err_t config_domain_joint_cal_save_to_nvs(
    nvs_handle_t handle,
    const joint_calib_config_t* config
) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    char key[32];

    for (int leg = 0; leg < NUM_LEGS; leg++) {
        for (int joint = 0; joint < NUM_JOINTS_PER_LEG; joint++) {
            const char* joint_name = JOINT_NAMES[joint];
            const joint_calib_t* calib = &config->joints[leg][joint];

            snprintf(key, sizeof(key), JOINT_KEY_OFFSET_FORMAT, leg, joint_name);
            ESP_ERROR_CHECK(nvs_set_blob(handle, key, &calib->zero_offset_rad, sizeof(float)));

            snprintf(key, sizeof(key), JOINT_KEY_INVERT_FORMAT, leg, joint_name);
            ESP_ERROR_CHECK(nvs_set_i8(handle, key, calib->invert_sign));

            snprintf(key, sizeof(key), JOINT_KEY_MIN_FORMAT, leg, joint_name);
            ESP_ERROR_CHECK(nvs_set_blob(handle, key, &calib->min_rad, sizeof(float)));

            snprintf(key, sizeof(key), JOINT_KEY_MAX_FORMAT, leg, joint_name);
            ESP_ERROR_CHECK(nvs_set_blob(handle, key, &calib->max_rad, sizeof(float)));

            snprintf(key, sizeof(key), JOINT_KEY_PWM_MIN_FORMAT, leg, joint_name);
            ESP_ERROR_CHECK(nvs_set_i32(handle, key, calib->pwm_min_us));

            snprintf(key, sizeof(key), JOINT_KEY_PWM_MAX_FORMAT, leg, joint_name);
            ESP_ERROR_CHECK(nvs_set_i32(handle, key, calib->pwm_max_us));

            snprintf(key, sizeof(key), JOINT_KEY_NEUTRAL_FORMAT, leg, joint_name);
            ESP_ERROR_CHECK(nvs_set_i32(handle, key, calib->neutral_us));
        }
    }

    return nvs_commit(handle);
}
