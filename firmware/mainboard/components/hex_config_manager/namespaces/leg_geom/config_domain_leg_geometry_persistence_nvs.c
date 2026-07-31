#include "config_domain_persistence_nvs.h"

#include "config_domain_defaults.h"
#include "esp_log.h"

#include <stdio.h>

static const char *TAG = "cfg_leggeom_nvs";

#define LEG_GEOM_KEY_LEN_COXA_FORMAT   "l%d_lc"
#define LEG_GEOM_KEY_LEN_FEMUR_FORMAT  "l%d_lf"
#define LEG_GEOM_KEY_LEN_TIBIA_FORMAT  "l%d_lt"
#define LEG_GEOM_KEY_MOUNT_X_FORMAT    "l%d_mx"
#define LEG_GEOM_KEY_MOUNT_Y_FORMAT    "l%d_my"
#define LEG_GEOM_KEY_MOUNT_Z_FORMAT    "l%d_mz"
#define LEG_GEOM_KEY_MOUNT_YAW_FORMAT  "l%d_myaw"
#define LEG_GEOM_KEY_STANCE_OUT_FORMAT "l%d_so"
#define LEG_GEOM_KEY_STANCE_FWD_FORMAT "l%d_sf"

static esp_err_t nvs_set_float_blob(nvs_handle_t handle, const char* key, float value) {
    return nvs_set_blob(handle, key, &value, sizeof(float));
}

static esp_err_t nvs_get_float_blob(nvs_handle_t handle, const char* key, float* value_out) {
    size_t required_size = sizeof(float);
    return nvs_get_blob(handle, key, value_out, &required_size);
}

esp_err_t config_domain_leg_geometry_write_defaults_to_nvs(nvs_handle_t handle) {
    char key[32];
    leg_geometry_config_t defaults;

    config_load_leg_geometry_defaults(&defaults);

    ESP_LOGI(TAG, "Initializing leg geometry namespace defaults to NVS");

    for (int leg = 0; leg < NUM_LEGS; leg++) {
        snprintf(key, sizeof(key), LEG_GEOM_KEY_LEN_COXA_FORMAT, leg);
        ESP_ERROR_CHECK(nvs_set_float_blob(handle, key, defaults.len_coxa[leg]));

        snprintf(key, sizeof(key), LEG_GEOM_KEY_LEN_FEMUR_FORMAT, leg);
        ESP_ERROR_CHECK(nvs_set_float_blob(handle, key, defaults.len_femur[leg]));

        snprintf(key, sizeof(key), LEG_GEOM_KEY_LEN_TIBIA_FORMAT, leg);
        ESP_ERROR_CHECK(nvs_set_float_blob(handle, key, defaults.len_tibia[leg]));

        snprintf(key, sizeof(key), LEG_GEOM_KEY_MOUNT_X_FORMAT, leg);
        ESP_ERROR_CHECK(nvs_set_float_blob(handle, key, defaults.mount_x[leg]));

        snprintf(key, sizeof(key), LEG_GEOM_KEY_MOUNT_Y_FORMAT, leg);
        ESP_ERROR_CHECK(nvs_set_float_blob(handle, key, defaults.mount_y[leg]));

        snprintf(key, sizeof(key), LEG_GEOM_KEY_MOUNT_Z_FORMAT, leg);
        ESP_ERROR_CHECK(nvs_set_float_blob(handle, key, defaults.mount_z[leg]));

        snprintf(key, sizeof(key), LEG_GEOM_KEY_MOUNT_YAW_FORMAT, leg);
        ESP_ERROR_CHECK(nvs_set_float_blob(handle, key, defaults.mount_yaw[leg]));

        snprintf(key, sizeof(key), LEG_GEOM_KEY_STANCE_OUT_FORMAT, leg);
        ESP_ERROR_CHECK(nvs_set_float_blob(handle, key, defaults.stance_out[leg]));

        snprintf(key, sizeof(key), LEG_GEOM_KEY_STANCE_FWD_FORMAT, leg);
        ESP_ERROR_CHECK(nvs_set_float_blob(handle, key, defaults.stance_fwd[leg]));
    }

    ESP_ERROR_CHECK(nvs_commit(handle));
    ESP_LOGI(TAG, "Leg geometry defaults written to NVS");

    return ESP_OK;
}

esp_err_t config_domain_leg_geometry_load_from_nvs(
    nvs_handle_t handle,
    leg_geometry_config_t* config
) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err;
    char key[32];

    ESP_LOGI(TAG, "Loading leg geometry configuration from NVS (read-only)");

    for (int leg = 0; leg < NUM_LEGS; leg++) {
        snprintf(key, sizeof(key), LEG_GEOM_KEY_LEN_COXA_FORMAT, leg);
        err = nvs_get_float_blob(handle, key, &config->len_coxa[leg]);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Failed to load len_coxa for leg %d: %s", leg, esp_err_to_name(err));
        }

        snprintf(key, sizeof(key), LEG_GEOM_KEY_LEN_FEMUR_FORMAT, leg);
        err = nvs_get_float_blob(handle, key, &config->len_femur[leg]);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Failed to load len_femur for leg %d: %s", leg, esp_err_to_name(err));
        }

        snprintf(key, sizeof(key), LEG_GEOM_KEY_LEN_TIBIA_FORMAT, leg);
        err = nvs_get_float_blob(handle, key, &config->len_tibia[leg]);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Failed to load len_tibia for leg %d: %s", leg, esp_err_to_name(err));
        }

        snprintf(key, sizeof(key), LEG_GEOM_KEY_MOUNT_X_FORMAT, leg);
        err = nvs_get_float_blob(handle, key, &config->mount_x[leg]);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Failed to load mount_x for leg %d: %s", leg, esp_err_to_name(err));
        }

        snprintf(key, sizeof(key), LEG_GEOM_KEY_MOUNT_Y_FORMAT, leg);
        err = nvs_get_float_blob(handle, key, &config->mount_y[leg]);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Failed to load mount_y for leg %d: %s", leg, esp_err_to_name(err));
        }

        snprintf(key, sizeof(key), LEG_GEOM_KEY_MOUNT_Z_FORMAT, leg);
        err = nvs_get_float_blob(handle, key, &config->mount_z[leg]);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Failed to load mount_z for leg %d: %s", leg, esp_err_to_name(err));
        }

        snprintf(key, sizeof(key), LEG_GEOM_KEY_MOUNT_YAW_FORMAT, leg);
        err = nvs_get_float_blob(handle, key, &config->mount_yaw[leg]);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Failed to load mount_yaw for leg %d: %s", leg, esp_err_to_name(err));
        }

        snprintf(key, sizeof(key), LEG_GEOM_KEY_STANCE_OUT_FORMAT, leg);
        err = nvs_get_float_blob(handle, key, &config->stance_out[leg]);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Failed to load stance_out for leg %d: %s", leg, esp_err_to_name(err));
        }

        snprintf(key, sizeof(key), LEG_GEOM_KEY_STANCE_FWD_FORMAT, leg);
        err = nvs_get_float_blob(handle, key, &config->stance_fwd[leg]);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Failed to load stance_fwd for leg %d: %s", leg, esp_err_to_name(err));
        }
    }

    return ESP_OK;
}

esp_err_t config_domain_leg_geometry_save_to_nvs(
    nvs_handle_t handle,
    const leg_geometry_config_t* config
) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    char key[32];

    for (int leg = 0; leg < NUM_LEGS; leg++) {
        snprintf(key, sizeof(key), LEG_GEOM_KEY_LEN_COXA_FORMAT, leg);
        ESP_ERROR_CHECK(nvs_set_float_blob(handle, key, config->len_coxa[leg]));

        snprintf(key, sizeof(key), LEG_GEOM_KEY_LEN_FEMUR_FORMAT, leg);
        ESP_ERROR_CHECK(nvs_set_float_blob(handle, key, config->len_femur[leg]));

        snprintf(key, sizeof(key), LEG_GEOM_KEY_LEN_TIBIA_FORMAT, leg);
        ESP_ERROR_CHECK(nvs_set_float_blob(handle, key, config->len_tibia[leg]));

        snprintf(key, sizeof(key), LEG_GEOM_KEY_MOUNT_X_FORMAT, leg);
        ESP_ERROR_CHECK(nvs_set_float_blob(handle, key, config->mount_x[leg]));

        snprintf(key, sizeof(key), LEG_GEOM_KEY_MOUNT_Y_FORMAT, leg);
        ESP_ERROR_CHECK(nvs_set_float_blob(handle, key, config->mount_y[leg]));

        snprintf(key, sizeof(key), LEG_GEOM_KEY_MOUNT_Z_FORMAT, leg);
        ESP_ERROR_CHECK(nvs_set_float_blob(handle, key, config->mount_z[leg]));

        snprintf(key, sizeof(key), LEG_GEOM_KEY_MOUNT_YAW_FORMAT, leg);
        ESP_ERROR_CHECK(nvs_set_float_blob(handle, key, config->mount_yaw[leg]));

        snprintf(key, sizeof(key), LEG_GEOM_KEY_STANCE_OUT_FORMAT, leg);
        ESP_ERROR_CHECK(nvs_set_float_blob(handle, key, config->stance_out[leg]));

        snprintf(key, sizeof(key), LEG_GEOM_KEY_STANCE_FWD_FORMAT, leg);
        ESP_ERROR_CHECK(nvs_set_float_blob(handle, key, config->stance_fwd[leg]));
    }

    return nvs_commit(handle);
}
