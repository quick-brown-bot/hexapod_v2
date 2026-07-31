#include "config_domain_joint_access.h"

#include <stdio.h>
#include <string.h>

static const char* JOINT_SHORT_NAMES[] = { "c", "f", "t" };
static const char* JOINT_FULL_NAMES[] = { "coxa", "femur", "tibia" };

typedef enum {
    JOINT_PARAM_OFFSET = 0,
    JOINT_PARAM_INVERT,
    JOINT_PARAM_MIN,
    JOINT_PARAM_MAX,
    JOINT_PARAM_PWM_MIN,
    JOINT_PARAM_PWM_MAX,
    JOINT_PARAM_NEUTRAL,
    JOINT_PARAM_INVALID
} joint_param_type_t;

static esp_err_t parse_joint_param_name(
    const char* param_name,
    int* leg_index,
    int* joint_index,
    joint_param_type_t* param_type,
    const char** param_suffix
) {
    if (!param_name || !leg_index || !joint_index || !param_type) {
        return ESP_ERR_INVALID_ARG;
    }

    int leg = 0;
    int joint_found = -1;
    char joint_name[16];
    char suffix[16];

    int parsed = sscanf(param_name, "leg%d_%15[^_]_%15s", &leg, joint_name, suffix);
    if (parsed != 3) {
        return ESP_ERR_INVALID_ARG;
    }

    if (leg < 0 || leg >= NUM_LEGS) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < NUM_JOINTS_PER_LEG; i++) {
        if (strcmp(joint_name, JOINT_FULL_NAMES[i]) == 0 || strcmp(joint_name, JOINT_SHORT_NAMES[i]) == 0) {
            joint_found = i;
            break;
        }
    }
    if (joint_found < 0) {
        return ESP_ERR_INVALID_ARG;
    }

    joint_param_type_t type = JOINT_PARAM_INVALID;
    const char* canonical_suffix = NULL;
    if (strcmp(suffix, "offset") == 0) {
        type = JOINT_PARAM_OFFSET;
        canonical_suffix = "offset";
    } else if (strcmp(suffix, "invert") == 0) {
        type = JOINT_PARAM_INVERT;
        canonical_suffix = "invert";
    } else if (strcmp(suffix, "min") == 0) {
        type = JOINT_PARAM_MIN;
        canonical_suffix = "min";
    } else if (strcmp(suffix, "max") == 0) {
        type = JOINT_PARAM_MAX;
        canonical_suffix = "max";
    } else if (strcmp(suffix, "pwm_min") == 0) {
        type = JOINT_PARAM_PWM_MIN;
        canonical_suffix = "pwm_min";
    } else if (strcmp(suffix, "pwm_max") == 0) {
        type = JOINT_PARAM_PWM_MAX;
        canonical_suffix = "pwm_max";
    } else if (strcmp(suffix, "neutral") == 0) {
        type = JOINT_PARAM_NEUTRAL;
        canonical_suffix = "neutral";
    } else {
        return ESP_ERR_INVALID_ARG;
    }

    *leg_index = leg;
    *joint_index = joint_found;
    *param_type = type;
    if (param_suffix) {
        *param_suffix = canonical_suffix;
    }

    return ESP_OK;
}

esp_err_t config_domain_joint_get_int32(
    const joint_calib_config_t* config,
    const char* param_name,
    int32_t* value
) {
    if (!config || !param_name || !value) {
        return ESP_ERR_INVALID_ARG;
    }

    int leg_index = 0;
    int joint_index = 0;
    joint_param_type_t param_type = JOINT_PARAM_INVALID;
    esp_err_t err = parse_joint_param_name(param_name, &leg_index, &joint_index, &param_type, NULL);
    if (err != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }

    const joint_calib_t* calib = &config->joints[leg_index][joint_index];

    switch (param_type) {
        case JOINT_PARAM_INVERT:
            *value = calib->invert_sign;
            break;
        case JOINT_PARAM_PWM_MIN:
            *value = calib->pwm_min_us;
            break;
        case JOINT_PARAM_PWM_MAX:
            *value = calib->pwm_max_us;
            break;
        case JOINT_PARAM_NEUTRAL:
            *value = calib->neutral_us;
            break;
        default:
            return ESP_ERR_NOT_FOUND;
    }

    return ESP_OK;
}

esp_err_t config_domain_joint_set_int32(
    joint_calib_config_t* config,
    const char* param_name,
    int32_t value
) {
    if (!config || !param_name) {
        return ESP_ERR_INVALID_ARG;
    }

    int leg_index = 0;
    int joint_index = 0;
    joint_param_type_t param_type = JOINT_PARAM_INVALID;
    esp_err_t err = parse_joint_param_name(param_name, &leg_index, &joint_index, &param_type, NULL);
    if (err != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }

    joint_calib_t* calib = &config->joints[leg_index][joint_index];

    switch (param_type) {
        case JOINT_PARAM_INVERT:
            if (value != -1 && value != 1) {
                return ESP_ERR_INVALID_ARG;
            }
            calib->invert_sign = (int8_t)value;
            break;
        case JOINT_PARAM_PWM_MIN:
            if (value < 500 || value > 3000) {
                return ESP_ERR_INVALID_ARG;
            }
            calib->pwm_min_us = value;
            break;
        case JOINT_PARAM_PWM_MAX:
            if (value < 500 || value > 3000) {
                return ESP_ERR_INVALID_ARG;
            }
            calib->pwm_max_us = value;
            break;
        case JOINT_PARAM_NEUTRAL:
            if (value < 500 || value > 3000) {
                return ESP_ERR_INVALID_ARG;
            }
            calib->neutral_us = value;
            break;
        default:
            return ESP_ERR_NOT_FOUND;
    }

    return ESP_OK;
}

esp_err_t config_domain_joint_get_float(
    const joint_calib_config_t* config,
    const char* param_name,
    float* value
) {
    if (!config || !param_name || !value) {
        return ESP_ERR_INVALID_ARG;
    }

    int leg_index = 0;
    int joint_index = 0;
    joint_param_type_t param_type = JOINT_PARAM_INVALID;
    esp_err_t err = parse_joint_param_name(param_name, &leg_index, &joint_index, &param_type, NULL);
    if (err != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }

    const joint_calib_t* calib = &config->joints[leg_index][joint_index];

    switch (param_type) {
        case JOINT_PARAM_OFFSET:
            *value = calib->zero_offset_rad;
            break;
        case JOINT_PARAM_MIN:
            *value = calib->min_rad;
            break;
        case JOINT_PARAM_MAX:
            *value = calib->max_rad;
            break;
        default:
            return ESP_ERR_NOT_FOUND;
    }

    return ESP_OK;
}

esp_err_t config_domain_joint_set_float(
    joint_calib_config_t* config,
    const char* param_name,
    float value
) {
    if (!config || !param_name) {
        return ESP_ERR_INVALID_ARG;
    }

    int leg_index = 0;
    int joint_index = 0;
    joint_param_type_t param_type = JOINT_PARAM_INVALID;
    esp_err_t err = parse_joint_param_name(param_name, &leg_index, &joint_index, &param_type, NULL);
    if (err != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }

    bool is_angle = (param_type == JOINT_PARAM_OFFSET || param_type == JOINT_PARAM_MIN || param_type == JOINT_PARAM_MAX);
    if (is_angle && (value < -6.28f || value > 6.28f)) {
        return ESP_ERR_INVALID_ARG;
    }

    joint_calib_t* calib = &config->joints[leg_index][joint_index];

    switch (param_type) {
        case JOINT_PARAM_OFFSET:
            calib->zero_offset_rad = value;
            break;
        case JOINT_PARAM_MIN:
            calib->min_rad = value;
            break;
        case JOINT_PARAM_MAX:
            calib->max_rad = value;
            break;
        default:
            return ESP_ERR_NOT_FOUND;
    }

    return ESP_OK;
}

esp_err_t config_domain_joint_parse_param_name(
    const char* param_name,
    int* leg_index,
    int* joint_index,
    const char** param_suffix
) {
    joint_param_type_t param_type = JOINT_PARAM_INVALID;
    esp_err_t err = parse_joint_param_name(param_name, leg_index, joint_index, &param_type, param_suffix);
    if (err != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }

    return ESP_OK;
}
