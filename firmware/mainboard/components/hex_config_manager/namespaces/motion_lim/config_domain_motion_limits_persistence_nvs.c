#include "config_domain_persistence_nvs.h"

#include "config_domain_defaults.h"
#include "esp_log.h"

#include <stdio.h>

static const char *TAG = "cfg_motion_nvs";

#define MOT_KEY_MAX_VEL_FORMAT   "mv_%s"
#define MOT_KEY_MAX_ACC_FORMAT   "ma_%s"
#define MOT_KEY_MAX_JERK_FORMAT  "mj_%s"
#define MOT_KEY_VEL_FILT_ALPHA   "vf_a"
#define MOT_KEY_ACC_FILT_ALPHA   "af_a"
#define MOT_KEY_LEG_VF_ALPHA     "lvf_a"
#define MOT_KEY_BODY_VF_ALPHA    "bvf_a"
#define MOT_KEY_BODY_P_ALPHA     "bp_a"
#define MOT_KEY_BODY_R_ALPHA     "br_a"
#define MOT_KEY_FRONT_BACK_DIST  "fb_d"
#define MOT_KEY_LEFT_RIGHT_DIST  "lr_d"
#define MOT_KEY_MAX_LEG_VEL      "ml_v"
#define MOT_KEY_MAX_BODY_VEL     "mb_v"
#define MOT_KEY_MAX_ANG_VEL      "ma_v"
#define MOT_KEY_MIN_DT           "min_dt"
#define MOT_KEY_MAX_DT           "max_dt"
#define MOT_KEY_BODY_OFF_X       "bo_x"
#define MOT_KEY_BODY_OFF_Y       "bo_y"
#define MOT_KEY_BODY_OFF_Z       "bo_z"

static const char* JOINT_NAMES[] = { "c", "f", "t" };

static esp_err_t nvs_set_float_blob(nvs_handle_t handle, const char* key, float value) {
    return nvs_set_blob(handle, key, &value, sizeof(float));
}

static esp_err_t nvs_get_float_blob(nvs_handle_t handle, const char* key, float* value_out) {
    size_t required_size = sizeof(float);
    return nvs_get_blob(handle, key, value_out, &required_size);
}

esp_err_t config_domain_motion_limits_write_defaults_to_nvs(nvs_handle_t handle) {
    char key[32];
    motion_limits_config_t defaults;

    config_load_motion_limits_defaults(&defaults);

    ESP_LOGI(TAG, "Initializing motion limits namespace defaults to NVS");

    for (int joint = 0; joint < NUM_JOINTS_PER_LEG; joint++) {
        const char* joint_name = JOINT_NAMES[joint];

        snprintf(key, sizeof(key), MOT_KEY_MAX_VEL_FORMAT, joint_name);
        ESP_ERROR_CHECK(nvs_set_float_blob(handle, key, defaults.max_velocity[joint]));

        snprintf(key, sizeof(key), MOT_KEY_MAX_ACC_FORMAT, joint_name);
        ESP_ERROR_CHECK(nvs_set_float_blob(handle, key, defaults.max_acceleration[joint]));

        snprintf(key, sizeof(key), MOT_KEY_MAX_JERK_FORMAT, joint_name);
        ESP_ERROR_CHECK(nvs_set_float_blob(handle, key, defaults.max_jerk[joint]));
    }

    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_VEL_FILT_ALPHA, defaults.velocity_filter_alpha));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_ACC_FILT_ALPHA, defaults.accel_filter_alpha));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_LEG_VF_ALPHA, defaults.leg_velocity_filter_alpha));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_BODY_VF_ALPHA, defaults.body_velocity_filter_alpha));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_BODY_P_ALPHA, defaults.body_pitch_filter_alpha));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_BODY_R_ALPHA, defaults.body_roll_filter_alpha));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_FRONT_BACK_DIST, defaults.front_to_back_distance));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_LEFT_RIGHT_DIST, defaults.left_to_right_distance));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_MAX_LEG_VEL, defaults.max_leg_velocity));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_MAX_BODY_VEL, defaults.max_body_velocity));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_MAX_ANG_VEL, defaults.max_angular_velocity));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_MIN_DT, defaults.min_dt));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_MAX_DT, defaults.max_dt));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_BODY_OFF_X, defaults.body_offset_x));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_BODY_OFF_Y, defaults.body_offset_y));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_BODY_OFF_Z, defaults.body_offset_z));

    ESP_ERROR_CHECK(nvs_commit(handle));
    ESP_LOGI(TAG, "Motion limits defaults written to NVS");

    return ESP_OK;
}

esp_err_t config_domain_motion_limits_load_from_nvs(
    nvs_handle_t handle,
    motion_limits_config_t* config
) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err;
    char key[32];

    ESP_LOGI(TAG, "Loading motion limits configuration from NVS (read-only)");

    for (int joint = 0; joint < NUM_JOINTS_PER_LEG; joint++) {
        const char* joint_name = JOINT_NAMES[joint];

        snprintf(key, sizeof(key), MOT_KEY_MAX_VEL_FORMAT, joint_name);
        err = nvs_get_float_blob(handle, key, &config->max_velocity[joint]);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Failed to load max_velocity[%d]: %s", joint, esp_err_to_name(err));
        }

        snprintf(key, sizeof(key), MOT_KEY_MAX_ACC_FORMAT, joint_name);
        err = nvs_get_float_blob(handle, key, &config->max_acceleration[joint]);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Failed to load max_acceleration[%d]: %s", joint, esp_err_to_name(err));
        }

        snprintf(key, sizeof(key), MOT_KEY_MAX_JERK_FORMAT, joint_name);
        err = nvs_get_float_blob(handle, key, &config->max_jerk[joint]);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Failed to load max_jerk[%d]: %s", joint, esp_err_to_name(err));
        }
    }

    err = nvs_get_float_blob(handle, MOT_KEY_VEL_FILT_ALPHA, &config->velocity_filter_alpha);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load velocity_filter_alpha: %s", esp_err_to_name(err));
    }

    err = nvs_get_float_blob(handle, MOT_KEY_ACC_FILT_ALPHA, &config->accel_filter_alpha);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load accel_filter_alpha: %s", esp_err_to_name(err));
    }

    err = nvs_get_float_blob(handle, MOT_KEY_LEG_VF_ALPHA, &config->leg_velocity_filter_alpha);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load leg_velocity_filter_alpha: %s", esp_err_to_name(err));
    }

    err = nvs_get_float_blob(handle, MOT_KEY_BODY_VF_ALPHA, &config->body_velocity_filter_alpha);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load body_velocity_filter_alpha: %s", esp_err_to_name(err));
    }

    err = nvs_get_float_blob(handle, MOT_KEY_BODY_P_ALPHA, &config->body_pitch_filter_alpha);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load body_pitch_filter_alpha: %s", esp_err_to_name(err));
    }

    err = nvs_get_float_blob(handle, MOT_KEY_BODY_R_ALPHA, &config->body_roll_filter_alpha);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load body_roll_filter_alpha: %s", esp_err_to_name(err));
    }

    err = nvs_get_float_blob(handle, MOT_KEY_FRONT_BACK_DIST, &config->front_to_back_distance);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load front_to_back_distance: %s", esp_err_to_name(err));
    }

    err = nvs_get_float_blob(handle, MOT_KEY_LEFT_RIGHT_DIST, &config->left_to_right_distance);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load left_to_right_distance: %s", esp_err_to_name(err));
    }

    err = nvs_get_float_blob(handle, MOT_KEY_MAX_LEG_VEL, &config->max_leg_velocity);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load max_leg_velocity: %s", esp_err_to_name(err));
    }

    err = nvs_get_float_blob(handle, MOT_KEY_MAX_BODY_VEL, &config->max_body_velocity);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load max_body_velocity: %s", esp_err_to_name(err));
    }

    err = nvs_get_float_blob(handle, MOT_KEY_MAX_ANG_VEL, &config->max_angular_velocity);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load max_angular_velocity: %s", esp_err_to_name(err));
    }

    err = nvs_get_float_blob(handle, MOT_KEY_MIN_DT, &config->min_dt);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load min_dt: %s", esp_err_to_name(err));
    }

    err = nvs_get_float_blob(handle, MOT_KEY_MAX_DT, &config->max_dt);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load max_dt: %s", esp_err_to_name(err));
    }

    err = nvs_get_float_blob(handle, MOT_KEY_BODY_OFF_X, &config->body_offset_x);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load body_offset_x: %s", esp_err_to_name(err));
    }

    err = nvs_get_float_blob(handle, MOT_KEY_BODY_OFF_Y, &config->body_offset_y);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load body_offset_y: %s", esp_err_to_name(err));
    }

    err = nvs_get_float_blob(handle, MOT_KEY_BODY_OFF_Z, &config->body_offset_z);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load body_offset_z: %s", esp_err_to_name(err));
    }

    return ESP_OK;
}

esp_err_t config_domain_motion_limits_save_to_nvs(
    nvs_handle_t handle,
    const motion_limits_config_t* config
) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    char key[32];

    for (int joint = 0; joint < NUM_JOINTS_PER_LEG; joint++) {
        const char* joint_name = JOINT_NAMES[joint];

        snprintf(key, sizeof(key), MOT_KEY_MAX_VEL_FORMAT, joint_name);
        ESP_ERROR_CHECK(nvs_set_float_blob(handle, key, config->max_velocity[joint]));

        snprintf(key, sizeof(key), MOT_KEY_MAX_ACC_FORMAT, joint_name);
        ESP_ERROR_CHECK(nvs_set_float_blob(handle, key, config->max_acceleration[joint]));

        snprintf(key, sizeof(key), MOT_KEY_MAX_JERK_FORMAT, joint_name);
        ESP_ERROR_CHECK(nvs_set_float_blob(handle, key, config->max_jerk[joint]));
    }

    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_VEL_FILT_ALPHA, config->velocity_filter_alpha));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_ACC_FILT_ALPHA, config->accel_filter_alpha));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_LEG_VF_ALPHA, config->leg_velocity_filter_alpha));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_BODY_VF_ALPHA, config->body_velocity_filter_alpha));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_BODY_P_ALPHA, config->body_pitch_filter_alpha));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_BODY_R_ALPHA, config->body_roll_filter_alpha));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_FRONT_BACK_DIST, config->front_to_back_distance));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_LEFT_RIGHT_DIST, config->left_to_right_distance));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_MAX_LEG_VEL, config->max_leg_velocity));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_MAX_BODY_VEL, config->max_body_velocity));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_MAX_ANG_VEL, config->max_angular_velocity));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_MIN_DT, config->min_dt));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_MAX_DT, config->max_dt));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_BODY_OFF_X, config->body_offset_x));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_BODY_OFF_Y, config->body_offset_y));
    ESP_ERROR_CHECK(nvs_set_float_blob(handle, MOT_KEY_BODY_OFF_Z, config->body_offset_z));

    return nvs_commit(handle);
}
