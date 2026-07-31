#include "config_domain_persistence_nvs.h"

#include "config_domain_defaults.h"
#include "esp_log.h"

static const char *TAG = "cfg_gait_nvs";

#define GAIT_KEY_CYCLE_TIME_S       "cycle_s"
#define GAIT_KEY_STEP_LENGTH_M      "step_m"
#define GAIT_KEY_CLEARANCE_HEIGHT_M "clr_m"
#define GAIT_KEY_Y_RANGE_M          "yrng_m"
#define GAIT_KEY_Z_MIN_M            "zmin_m"
#define GAIT_KEY_Z_MAX_M            "zmax_m"
#define GAIT_KEY_MAX_YAW_PER_CYCLE  "yaw_cyc"
#define GAIT_KEY_TURN_DIRECTION     "turn_dir"

static esp_err_t nvs_set_float_blob(nvs_handle_t handle, const char* key, float value) {
    return nvs_set_blob(handle, key, &value, sizeof(float));
}

static esp_err_t nvs_get_float_blob(nvs_handle_t handle, const char* key, float* value_out) {
    size_t required_size = sizeof(float);
    return nvs_get_blob(handle, key, value_out, &required_size);
}

esp_err_t config_domain_gait_write_defaults_to_nvs(nvs_handle_t handle) {
    gait_config_t defaults;

    config_load_gait_defaults(&defaults);

    ESP_LOGI(TAG, "Initializing gait namespace defaults to NVS");

    ESP_ERROR_CHECK(nvs_set_float_blob(handle, GAIT_KEY_CYCLE_TIME_S, defaults.cycle_time_s));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, GAIT_KEY_STEP_LENGTH_M, defaults.step_length_m));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, GAIT_KEY_CLEARANCE_HEIGHT_M, defaults.clearance_height_m));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, GAIT_KEY_Y_RANGE_M, defaults.y_range_m));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, GAIT_KEY_Z_MIN_M, defaults.z_min_m));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, GAIT_KEY_Z_MAX_M, defaults.z_max_m));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, GAIT_KEY_MAX_YAW_PER_CYCLE, defaults.max_yaw_per_cycle_rad));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, GAIT_KEY_TURN_DIRECTION, defaults.turn_direction));

    ESP_ERROR_CHECK(nvs_commit(handle));
    ESP_LOGI(TAG, "Gait defaults written to NVS");

    return ESP_OK;
}

esp_err_t config_domain_gait_load_from_nvs(
    nvs_handle_t handle,
    gait_config_t* config
) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err;

    ESP_LOGI(TAG, "Loading gait configuration from NVS (read-only)");

    err = nvs_get_float_blob(handle, GAIT_KEY_CYCLE_TIME_S, &config->cycle_time_s);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load cycle_time_s: %s", esp_err_to_name(err));
    }

    err = nvs_get_float_blob(handle, GAIT_KEY_STEP_LENGTH_M, &config->step_length_m);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load step_length_m: %s", esp_err_to_name(err));
    }

    err = nvs_get_float_blob(handle, GAIT_KEY_CLEARANCE_HEIGHT_M, &config->clearance_height_m);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load clearance_height_m: %s", esp_err_to_name(err));
    }

    err = nvs_get_float_blob(handle, GAIT_KEY_Y_RANGE_M, &config->y_range_m);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load y_range_m: %s", esp_err_to_name(err));
    }

    err = nvs_get_float_blob(handle, GAIT_KEY_Z_MIN_M, &config->z_min_m);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load z_min_m: %s", esp_err_to_name(err));
    }

    err = nvs_get_float_blob(handle, GAIT_KEY_Z_MAX_M, &config->z_max_m);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load z_max_m: %s", esp_err_to_name(err));
    }

    err = nvs_get_float_blob(handle, GAIT_KEY_MAX_YAW_PER_CYCLE, &config->max_yaw_per_cycle_rad);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load max_yaw_per_cycle_rad: %s", esp_err_to_name(err));
    }

    err = nvs_get_float_blob(handle, GAIT_KEY_TURN_DIRECTION, &config->turn_direction);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load turn_direction: %s", esp_err_to_name(err));
    }

    return ESP_OK;
}

esp_err_t config_domain_gait_save_to_nvs(
    nvs_handle_t handle,
    const gait_config_t* config
) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_ERROR_CHECK(nvs_set_float_blob(handle, GAIT_KEY_CYCLE_TIME_S, config->cycle_time_s));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, GAIT_KEY_STEP_LENGTH_M, config->step_length_m));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, GAIT_KEY_CLEARANCE_HEIGHT_M, config->clearance_height_m));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, GAIT_KEY_Y_RANGE_M, config->y_range_m));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, GAIT_KEY_Z_MIN_M, config->z_min_m));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, GAIT_KEY_Z_MAX_M, config->z_max_m));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, GAIT_KEY_MAX_YAW_PER_CYCLE, config->max_yaw_per_cycle_rad));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, GAIT_KEY_TURN_DIRECTION, config->turn_direction));

    return nvs_commit(handle);
}
