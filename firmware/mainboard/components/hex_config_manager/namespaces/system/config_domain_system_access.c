#include "config_domain_system_access.h"

#include <string.h>
#include <stddef.h>

static const config_param_info_t g_system_param_table[] = {
    {
        .name = "emergency_stop_enabled",
        .type = CONFIG_TYPE_BOOL,
        .offset = offsetof(system_config_t, emergency_stop_enabled),
        .size = sizeof(bool),
        .constraints = { .int_range = { 0, 1 } }
    },
    {
        .name = "auto_disarm_timeout",
        .type = CONFIG_TYPE_UINT32,
        .offset = offsetof(system_config_t, auto_disarm_timeout),
        .size = sizeof(uint32_t),
        .constraints = { .uint_range = { 5, 300 } }
    },
    {
        .name = "safety_voltage_min",
        .type = CONFIG_TYPE_FLOAT,
        .offset = offsetof(system_config_t, safety_voltage_min),
        .size = sizeof(float),
        .constraints = { .float_range = { 3.0f, 12.0f } }
    },
    {
        .name = "temperature_limit_max",
        .type = CONFIG_TYPE_FLOAT,
        .offset = offsetof(system_config_t, temperature_limit_max),
        .size = sizeof(float),
        .constraints = { .float_range = { 40.0f, 100.0f } }
    },
    {
        .name = "motion_timeout_ms",
        .type = CONFIG_TYPE_UINT32,
        .offset = offsetof(system_config_t, motion_timeout_ms),
        .size = sizeof(uint32_t),
        .constraints = { .uint_range = { 100, 5000 } }
    },
    {
        .name = "startup_delay_ms",
        .type = CONFIG_TYPE_UINT32,
        .offset = offsetof(system_config_t, startup_delay_ms),
        .size = sizeof(uint32_t),
        .constraints = { .uint_range = { 0, 10000 } }
    },
    {
        .name = "max_control_frequency",
        .type = CONFIG_TYPE_UINT32,
        .offset = offsetof(system_config_t, max_control_frequency),
        .size = sizeof(uint32_t),
        .constraints = { .uint_range = { 50, 1000 } }
    },
    {
        .name = "robot_id",
        .type = CONFIG_TYPE_STRING,
        .offset = offsetof(system_config_t, robot_id),
        .size = sizeof(((system_config_t*)0)->robot_id),
        .constraints = { .string = { 32 } }
    },
    {
        .name = "robot_name",
        .type = CONFIG_TYPE_STRING,
        .offset = offsetof(system_config_t, robot_name),
        .size = sizeof(((system_config_t*)0)->robot_name),
        .constraints = { .string = { 64 } }
    },
    {
        .name = "config_version",
        .type = CONFIG_TYPE_UINT16,
        .offset = offsetof(system_config_t, config_version),
        .size = sizeof(uint16_t),
        .constraints = { .uint_range = { 1, 65535 } }
    },
    {
        .name = "controller_type",
        .type = CONFIG_TYPE_UINT16,
        .offset = offsetof(system_config_t, controller_type),
        .size = sizeof(controller_driver_type_e),
        .constraints = { .uint_range = { 0, 4 } }
    }
};

static const size_t g_system_param_count = sizeof(g_system_param_table) / sizeof(g_system_param_table[0]);

static const config_param_info_t* find_system_param(const char* param_name) {
    if (!param_name) {
        return NULL;
    }

    for (size_t i = 0; i < g_system_param_count; i++) {
        if (strcmp(g_system_param_table[i].name, param_name) == 0) {
            return &g_system_param_table[i];
        }
    }

    return NULL;
}

static void* get_param_ptr(system_config_t* config, const config_param_info_t* param) {
    return (uint8_t*)config + param->offset;
}

static const void* get_const_param_ptr(const system_config_t* config, const config_param_info_t* param) {
    return (const uint8_t*)config + param->offset;
}

esp_err_t config_domain_system_list_parameters(
    const char** param_names,
    size_t max_params,
    size_t* count
) {
    if (!param_names || !count) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t params_to_copy = (g_system_param_count < max_params) ? g_system_param_count : max_params;
    for (size_t i = 0; i < params_to_copy; i++) {
        param_names[i] = g_system_param_table[i].name;
    }

    *count = params_to_copy;
    return ESP_OK;
}

esp_err_t config_domain_system_get_param_info(
    const char* param_name,
    config_param_info_t* info
) {
    if (!param_name || !info) {
        return ESP_ERR_INVALID_ARG;
    }

    const config_param_info_t* param = find_system_param(param_name);
    if (!param) {
        return ESP_ERR_NOT_FOUND;
    }

    memcpy(info, param, sizeof(config_param_info_t));
    return ESP_OK;
}

esp_err_t config_domain_system_get_raw(
    const system_config_t* config,
    const char* param_name,
    void* value_out,
    size_t value_size
) {
    if (!config || !param_name || !value_out) {
        return ESP_ERR_INVALID_ARG;
    }

    const config_param_info_t* param = find_system_param(param_name);
    if (!param) {
        return ESP_ERR_NOT_FOUND;
    }

    if (value_size < param->size) {
        return ESP_ERR_INVALID_SIZE;
    }

    memcpy(value_out, get_const_param_ptr(config, param), param->size);
    return ESP_OK;
}

esp_err_t config_domain_system_get_bool(
    const system_config_t* config,
    const char* param_name,
    bool* value
) {
    if (!config || !param_name || !value) {
        return ESP_ERR_INVALID_ARG;
    }

    const config_param_info_t* param = find_system_param(param_name);
    if (!param || param->type != CONFIG_TYPE_BOOL) {
        return ESP_ERR_NOT_FOUND;
    }

    *value = *(const bool*)get_const_param_ptr(config, param);
    return ESP_OK;
}

esp_err_t config_domain_system_set_bool(
    system_config_t* config,
    const char* param_name,
    bool value
) {
    if (!config || !param_name) {
        return ESP_ERR_INVALID_ARG;
    }

    const config_param_info_t* param = find_system_param(param_name);
    if (!param || param->type != CONFIG_TYPE_BOOL) {
        return ESP_ERR_NOT_FOUND;
    }

    *(bool*)get_param_ptr(config, param) = value;
    return ESP_OK;
}

esp_err_t config_domain_system_get_int32(
    const system_config_t* config,
    const char* param_name,
    int32_t* value
) {
    if (!config || !param_name || !value) {
        return ESP_ERR_INVALID_ARG;
    }

    const config_param_info_t* param = find_system_param(param_name);
    if (!param || param->type != CONFIG_TYPE_INT32) {
        return ESP_ERR_NOT_FOUND;
    }

    *value = *(const int32_t*)get_const_param_ptr(config, param);
    return ESP_OK;
}

esp_err_t config_domain_system_set_int32(
    system_config_t* config,
    const char* param_name,
    int32_t value
) {
    if (!config || !param_name) {
        return ESP_ERR_INVALID_ARG;
    }

    const config_param_info_t* param = find_system_param(param_name);
    if (!param || param->type != CONFIG_TYPE_INT32) {
        return ESP_ERR_NOT_FOUND;
    }

    if (value < param->constraints.int_range.min || value > param->constraints.int_range.max) {
        return ESP_ERR_INVALID_ARG;
    }

    *(int32_t*)get_param_ptr(config, param) = value;
    return ESP_OK;
}

esp_err_t config_domain_system_get_uint32(
    const system_config_t* config,
    const char* param_name,
    uint32_t* value
) {
    if (!config || !param_name || !value) {
        return ESP_ERR_INVALID_ARG;
    }

    const config_param_info_t* param = find_system_param(param_name);
    if (!param || (param->type != CONFIG_TYPE_UINT32 && param->type != CONFIG_TYPE_UINT16)) {
        return ESP_ERR_NOT_FOUND;
    }

    if (param->type == CONFIG_TYPE_UINT32) {
        *value = *(const uint32_t*)get_const_param_ptr(config, param);
    } else {
        *value = (uint32_t)*(const uint16_t*)get_const_param_ptr(config, param);
    }

    return ESP_OK;
}

esp_err_t config_domain_system_set_uint32(
    system_config_t* config,
    const char* param_name,
    uint32_t value
) {
    if (!config || !param_name) {
        return ESP_ERR_INVALID_ARG;
    }

    const config_param_info_t* param = find_system_param(param_name);
    if (!param || (param->type != CONFIG_TYPE_UINT32 && param->type != CONFIG_TYPE_UINT16)) {
        return ESP_ERR_NOT_FOUND;
    }

    if (value < param->constraints.uint_range.min || value > param->constraints.uint_range.max) {
        return ESP_ERR_INVALID_ARG;
    }

    if (param->type == CONFIG_TYPE_UINT32) {
        *(uint32_t*)get_param_ptr(config, param) = value;
    } else {
        *(uint16_t*)get_param_ptr(config, param) = (uint16_t)value;
    }

    return ESP_OK;
}

esp_err_t config_domain_system_get_float(
    const system_config_t* config,
    const char* param_name,
    float* value
) {
    if (!config || !param_name || !value) {
        return ESP_ERR_INVALID_ARG;
    }

    const config_param_info_t* param = find_system_param(param_name);
    if (!param || param->type != CONFIG_TYPE_FLOAT) {
        return ESP_ERR_NOT_FOUND;
    }

    *value = *(const float*)get_const_param_ptr(config, param);
    return ESP_OK;
}

esp_err_t config_domain_system_set_float(
    system_config_t* config,
    const char* param_name,
    float value
) {
    if (!config || !param_name) {
        return ESP_ERR_INVALID_ARG;
    }

    const config_param_info_t* param = find_system_param(param_name);
    if (!param || param->type != CONFIG_TYPE_FLOAT) {
        return ESP_ERR_NOT_FOUND;
    }

    if (value < param->constraints.float_range.min || value > param->constraints.float_range.max) {
        return ESP_ERR_INVALID_ARG;
    }

    *(float*)get_param_ptr(config, param) = value;
    return ESP_OK;
}

esp_err_t config_domain_system_get_string(
    const system_config_t* config,
    const char* param_name,
    char* value,
    size_t max_len
) {
    if (!config || !param_name || !value || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const config_param_info_t* param = find_system_param(param_name);
    if (!param || param->type != CONFIG_TYPE_STRING) {
        return ESP_ERR_NOT_FOUND;
    }

    const char* param_ptr = (const char*)get_const_param_ptr(config, param);
    strncpy(value, param_ptr, max_len - 1);
    value[max_len - 1] = '\0';
    return ESP_OK;
}

esp_err_t config_domain_system_set_string(
    system_config_t* config,
    const char* param_name,
    const char* value
) {
    if (!config || !param_name || !value) {
        return ESP_ERR_INVALID_ARG;
    }

    const config_param_info_t* param = find_system_param(param_name);
    if (!param || param->type != CONFIG_TYPE_STRING) {
        return ESP_ERR_NOT_FOUND;
    }

    size_t value_len = strlen(value);
    if (value_len >= param->constraints.string.max_length) {
        return ESP_ERR_INVALID_ARG;
    }

    char* param_ptr = (char*)get_param_ptr(config, param);
    strncpy(param_ptr, value, param->size - 1);
    param_ptr[param->size - 1] = '\0';

    return ESP_OK;
}
