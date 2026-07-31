#include "config_domain_controller_access.h"

#include <stddef.h>
#include <string.h>

typedef struct {
    const char* name;
    config_param_type_t type;
    size_t offset;
    size_t size;
    union {
        struct { int32_t min, max; } int_range;
        struct { uint32_t min, max; } uint_range;
        struct { float min, max; } float_range;
    } constraints;
} controller_param_meta_t;

static const controller_param_meta_t g_controller_param_table[] = {
    {
        .name = "driver_type",
        .type = CONFIG_TYPE_UINT32,
        .offset = offsetof(controller_config_namespace_t, driver_type),
        .size = sizeof(controller_driver_type_e),
        .constraints = { .uint_range = { 0, 4 } }
    },
    {
        .name = "task_stack",
        .type = CONFIG_TYPE_UINT32,
        .offset = offsetof(controller_config_namespace_t, task_stack),
        .size = sizeof(uint32_t),
        .constraints = { .uint_range = { 1024, 16384 } }
    },
    {
        .name = "task_priority",
        .type = CONFIG_TYPE_UINT32,
        .offset = offsetof(controller_config_namespace_t, task_priority),
        .size = sizeof(uint32_t),
        .constraints = { .uint_range = { 1, 24 } }
    },
    {
        .name = "stick_deadband",
        .type = CONFIG_TYPE_FLOAT,
        .offset = offsetof(controller_config_namespace_t, stick_deadband),
        .size = sizeof(float),
        .constraints = { .float_range = { 0.0f, 0.3f } }
    },
    {
        .name = "failsafe_vx",
        .type = CONFIG_TYPE_FLOAT,
        .offset = offsetof(controller_config_namespace_t, failsafe_vx),
        .size = sizeof(float),
        .constraints = { .float_range = { -1.0f, 1.0f } }
    },
    {
        .name = "failsafe_wz",
        .type = CONFIG_TYPE_FLOAT,
        .offset = offsetof(controller_config_namespace_t, failsafe_wz),
        .size = sizeof(float),
        .constraints = { .float_range = { -1.0f, 1.0f } }
    },
    {
        .name = "failsafe_z_target",
        .type = CONFIG_TYPE_FLOAT,
        .offset = offsetof(controller_config_namespace_t, failsafe_z_target),
        .size = sizeof(float),
        .constraints = { .float_range = { -1.0f, 1.0f } }
    },
    {
        .name = "failsafe_y_offset",
        .type = CONFIG_TYPE_FLOAT,
        .offset = offsetof(controller_config_namespace_t, failsafe_y_offset),
        .size = sizeof(float),
        .constraints = { .float_range = { -1.0f, 1.0f } }
    },
    {
        .name = "failsafe_step_scale",
        .type = CONFIG_TYPE_FLOAT,
        .offset = offsetof(controller_config_namespace_t, failsafe_step_scale),
        .size = sizeof(float),
        .constraints = { .float_range = { 0.0f, 1.0f } }
    },
    {
        .name = "failsafe_gait",
        .type = CONFIG_TYPE_INT32,
        .offset = offsetof(controller_config_namespace_t, failsafe_gait),
        .size = sizeof(int32_t),
        .constraints = { .int_range = { 0, 2 } }
    },
    {
        .name = "failsafe_enable",
        .type = CONFIG_TYPE_BOOL,
        .offset = offsetof(controller_config_namespace_t, failsafe_enable),
        .size = sizeof(bool),
    },
    {
        .name = "failsafe_pose_mode",
        .type = CONFIG_TYPE_BOOL,
        .offset = offsetof(controller_config_namespace_t, failsafe_pose_mode),
        .size = sizeof(bool),
    },
    {
        .name = "failsafe_terrain_climb",
        .type = CONFIG_TYPE_BOOL,
        .offset = offsetof(controller_config_namespace_t, failsafe_terrain_climb),
        .size = sizeof(bool),
    },
    {
        .name = "flysky_uart_port",
        .type = CONFIG_TYPE_INT32,
        .offset = offsetof(controller_config_namespace_t, flysky_uart_port),
        .size = sizeof(int32_t),
        .constraints = { .int_range = { 0, 2 } }
    },
    {
        .name = "flysky_tx_gpio",
        .type = CONFIG_TYPE_INT32,
        .offset = offsetof(controller_config_namespace_t, flysky_tx_gpio),
        .size = sizeof(int32_t),
        .constraints = { .int_range = { -1, 48 } }
    },
    {
        .name = "flysky_rx_gpio",
        .type = CONFIG_TYPE_INT32,
        .offset = offsetof(controller_config_namespace_t, flysky_rx_gpio),
        .size = sizeof(int32_t),
        .constraints = { .int_range = { 0, 48 } }
    },
    {
        .name = "flysky_rts_gpio",
        .type = CONFIG_TYPE_INT32,
        .offset = offsetof(controller_config_namespace_t, flysky_rts_gpio),
        .size = sizeof(int32_t),
        .constraints = { .int_range = { -1, 48 } }
    },
    {
        .name = "flysky_cts_gpio",
        .type = CONFIG_TYPE_INT32,
        .offset = offsetof(controller_config_namespace_t, flysky_cts_gpio),
        .size = sizeof(int32_t),
        .constraints = { .int_range = { -1, 48 } }
    },
    {
        .name = "flysky_baud_rate",
        .type = CONFIG_TYPE_INT32,
        .offset = offsetof(controller_config_namespace_t, flysky_baud_rate),
        .size = sizeof(int32_t),
        .constraints = { .int_range = { 1200, 1000000 } }
    }
};

static const size_t g_controller_param_count = sizeof(g_controller_param_table) / sizeof(g_controller_param_table[0]);

static const controller_param_meta_t* find_controller_param(const char* param_name) {
    if (!param_name) {
        return NULL;
    }

    for (size_t i = 0; i < g_controller_param_count; i++) {
        if (strcmp(g_controller_param_table[i].name, param_name) == 0) {
            return &g_controller_param_table[i];
        }
    }

    return NULL;
}

static void* get_param_ptr(controller_config_namespace_t* config, const controller_param_meta_t* param) {
    return (uint8_t*)config + param->offset;
}

static const void* get_const_param_ptr(const controller_config_namespace_t* config, const controller_param_meta_t* param) {
    return (const uint8_t*)config + param->offset;
}

esp_err_t config_domain_controller_list_parameters(
    const char** param_names,
    size_t max_params,
    size_t* count
) {
    if (!param_names || !count) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t params_to_copy = (g_controller_param_count < max_params) ? g_controller_param_count : max_params;
    for (size_t i = 0; i < params_to_copy; i++) {
        param_names[i] = g_controller_param_table[i].name;
    }

    *count = params_to_copy;
    return ESP_OK;
}

esp_err_t config_domain_controller_get_param_info(
    const char* param_name,
    config_param_info_t* info
) {
    if (!param_name || !info) {
        return ESP_ERR_INVALID_ARG;
    }

    const controller_param_meta_t* param = find_controller_param(param_name);
    if (!param) {
        return ESP_ERR_NOT_FOUND;
    }

    info->name = param->name;
    info->type = param->type;
    info->offset = param->offset;
    info->size = param->size;

    if (param->type == CONFIG_TYPE_INT32) {
        info->constraints.int_range.min = param->constraints.int_range.min;
        info->constraints.int_range.max = param->constraints.int_range.max;
    } else if (param->type == CONFIG_TYPE_FLOAT) {
        info->constraints.float_range.min = param->constraints.float_range.min;
        info->constraints.float_range.max = param->constraints.float_range.max;
    } else {
        info->constraints.uint_range.min = param->constraints.uint_range.min;
        info->constraints.uint_range.max = param->constraints.uint_range.max;
    }

    return ESP_OK;
}

esp_err_t config_domain_controller_get_int32(
    const controller_config_namespace_t* config,
    const char* param_name,
    int32_t* value
) {
    if (!config || !param_name || !value) {
        return ESP_ERR_INVALID_ARG;
    }

    const controller_param_meta_t* param = find_controller_param(param_name);
    if (!param || param->type != CONFIG_TYPE_INT32) {
        return ESP_ERR_NOT_FOUND;
    }

    *value = *(const int32_t*)get_const_param_ptr(config, param);
    return ESP_OK;
}

esp_err_t config_domain_controller_set_int32(
    controller_config_namespace_t* config,
    const char* param_name,
    int32_t value
) {
    if (!config || !param_name) {
        return ESP_ERR_INVALID_ARG;
    }

    const controller_param_meta_t* param = find_controller_param(param_name);
    if (!param || param->type != CONFIG_TYPE_INT32) {
        return ESP_ERR_NOT_FOUND;
    }

    if (value < param->constraints.int_range.min || value > param->constraints.int_range.max) {
        return ESP_ERR_INVALID_ARG;
    }

    *(int32_t*)get_param_ptr(config, param) = value;
    return ESP_OK;
}

esp_err_t config_domain_controller_get_uint32(
    const controller_config_namespace_t* config,
    const char* param_name,
    uint32_t* value
) {
    if (!config || !param_name || !value) {
        return ESP_ERR_INVALID_ARG;
    }

    const controller_param_meta_t* param = find_controller_param(param_name);
    if (!param || (param->type != CONFIG_TYPE_UINT16 && param->type != CONFIG_TYPE_UINT32)) {
        return ESP_ERR_NOT_FOUND;
    }

    if (param->type == CONFIG_TYPE_UINT16) {
        *value = (uint32_t)*(const uint16_t*)get_const_param_ptr(config, param);
    } else {
        *value = *(const uint32_t*)get_const_param_ptr(config, param);
    }

    return ESP_OK;
}

esp_err_t config_domain_controller_set_uint32(
    controller_config_namespace_t* config,
    const char* param_name,
    uint32_t value
) {
    if (!config || !param_name) {
        return ESP_ERR_INVALID_ARG;
    }

    const controller_param_meta_t* param = find_controller_param(param_name);
    if (!param || (param->type != CONFIG_TYPE_UINT16 && param->type != CONFIG_TYPE_UINT32)) {
        return ESP_ERR_NOT_FOUND;
    }

    if (value < param->constraints.uint_range.min || value > param->constraints.uint_range.max) {
        return ESP_ERR_INVALID_ARG;
    }

    if (param->type == CONFIG_TYPE_UINT16) {
        *(uint16_t*)get_param_ptr(config, param) = (uint16_t)value;
    } else {
        *(uint32_t*)get_param_ptr(config, param) = value;
    }

    return ESP_OK;
}

esp_err_t config_domain_controller_get_float(
    const controller_config_namespace_t* config,
    const char* param_name,
    float* value
) {
    if (!config || !param_name || !value) {
        return ESP_ERR_INVALID_ARG;
    }

    const controller_param_meta_t* param = find_controller_param(param_name);
    if (!param || param->type != CONFIG_TYPE_FLOAT) {
        return ESP_ERR_NOT_FOUND;
    }

    *value = *(const float*)get_const_param_ptr(config, param);
    return ESP_OK;
}

esp_err_t config_domain_controller_set_float(
    controller_config_namespace_t* config,
    const char* param_name,
    float value
) {
    if (!config || !param_name) {
        return ESP_ERR_INVALID_ARG;
    }

    const controller_param_meta_t* param = find_controller_param(param_name);
    if (!param || param->type != CONFIG_TYPE_FLOAT) {
        return ESP_ERR_NOT_FOUND;
    }

    if (value < param->constraints.float_range.min || value > param->constraints.float_range.max) {
        return ESP_ERR_INVALID_ARG;
    }

    *(float*)get_param_ptr(config, param) = value;
    return ESP_OK;
}

esp_err_t config_domain_controller_get_bool(
    const controller_config_namespace_t* config,
    const char* param_name,
    bool* value
) {
    if (!config || !param_name || !value) {
        return ESP_ERR_INVALID_ARG;
    }

    const controller_param_meta_t* param = find_controller_param(param_name);
    if (!param || param->type != CONFIG_TYPE_BOOL) {
        return ESP_ERR_NOT_FOUND;
    }

    *value = *(const bool*)get_const_param_ptr(config, param);
    return ESP_OK;
}

esp_err_t config_domain_controller_set_bool(
    controller_config_namespace_t* config,
    const char* param_name,
    bool value
) {
    if (!config || !param_name) {
        return ESP_ERR_INVALID_ARG;
    }

    const controller_param_meta_t* param = find_controller_param(param_name);
    if (!param || param->type != CONFIG_TYPE_BOOL) {
        return ESP_ERR_NOT_FOUND;
    }

    *(bool*)get_param_ptr(config, param) = value;
    return ESP_OK;
}
