#include "config_domain_wifi_access.h"

#include <stddef.h>
#include <string.h>

typedef struct {
    const char* name;
    config_param_type_t type;
    size_t offset;
    size_t size;
    union {
        struct { uint32_t min, max; } uint_range;
        struct { size_t max_length; } string;
    } constraints;
} wifi_param_meta_t;

static const wifi_param_meta_t g_wifi_param_table[] = {
    {
        .name = "ap_ssid_mode",
        .type = CONFIG_TYPE_UINT32,
        .offset = offsetof(wifi_config_namespace_t, ap_ssid_mode),
        .size = sizeof(uint32_t),
        .constraints = { .uint_range = { 0, 2 } }
    },
    {
        .name = "ap_fixed_prefix",
        .type = CONFIG_TYPE_STRING,
        .offset = offsetof(wifi_config_namespace_t, ap_fixed_prefix),
        .size = sizeof(((wifi_config_namespace_t*)0)->ap_fixed_prefix),
        .constraints = { .string = { sizeof(((wifi_config_namespace_t*)0)->ap_fixed_prefix) } }
    },
    {
        .name = "ap_fixed_ssid",
        .type = CONFIG_TYPE_STRING,
        .offset = offsetof(wifi_config_namespace_t, ap_fixed_ssid),
        .size = sizeof(((wifi_config_namespace_t*)0)->ap_fixed_ssid),
        .constraints = { .string = { sizeof(((wifi_config_namespace_t*)0)->ap_fixed_ssid) } }
    },
    {
        .name = "ap_password",
        .type = CONFIG_TYPE_STRING,
        .offset = offsetof(wifi_config_namespace_t, ap_password),
        .size = sizeof(((wifi_config_namespace_t*)0)->ap_password),
        .constraints = { .string = { sizeof(((wifi_config_namespace_t*)0)->ap_password) } }
    },
    {
        .name = "ap_channel",
        .type = CONFIG_TYPE_UINT32,
        .offset = offsetof(wifi_config_namespace_t, ap_channel),
        .size = sizeof(uint32_t),
        .constraints = { .uint_range = { 1, 13 } }
    },
    {
        .name = "ap_max_clients",
        .type = CONFIG_TYPE_UINT32,
        .offset = offsetof(wifi_config_namespace_t, ap_max_clients),
        .size = sizeof(uint32_t),
        .constraints = { .uint_range = { 1, 10 } }
    },
    {
        .name = "tcp_listen_port",
        .type = CONFIG_TYPE_UINT32,
        .offset = offsetof(wifi_config_namespace_t, tcp_listen_port),
        .size = sizeof(uint32_t),
        .constraints = { .uint_range = { 1, 65535 } }
    },
    {
        .name = "tcp_connection_timeout_ms",
        .type = CONFIG_TYPE_UINT32,
        .offset = offsetof(wifi_config_namespace_t, tcp_connection_timeout_ms),
        .size = sizeof(uint32_t),
        .constraints = { .uint_range = { 1000, 600000 } }
    }
};

static const size_t g_wifi_param_count = sizeof(g_wifi_param_table) / sizeof(g_wifi_param_table[0]);

static const wifi_param_meta_t* find_wifi_param(const char* param_name) {
    if (!param_name) {
        return NULL;
    }

    for (size_t i = 0; i < g_wifi_param_count; i++) {
        if (strcmp(g_wifi_param_table[i].name, param_name) == 0) {
            return &g_wifi_param_table[i];
        }
    }

    return NULL;
}

static void* get_param_ptr(wifi_config_namespace_t* config, const wifi_param_meta_t* param) {
    return (uint8_t*)config + param->offset;
}

static const void* get_const_param_ptr(const wifi_config_namespace_t* config, const wifi_param_meta_t* param) {
    return (const uint8_t*)config + param->offset;
}

esp_err_t config_domain_wifi_list_parameters(
    const char** param_names,
    size_t max_params,
    size_t* count
) {
    if (!param_names || !count) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t params_to_copy = (g_wifi_param_count < max_params) ? g_wifi_param_count : max_params;
    for (size_t i = 0; i < params_to_copy; i++) {
        param_names[i] = g_wifi_param_table[i].name;
    }

    *count = params_to_copy;
    return ESP_OK;
}

esp_err_t config_domain_wifi_get_param_info(
    const char* param_name,
    config_param_info_t* info
) {
    if (!param_name || !info) {
        return ESP_ERR_INVALID_ARG;
    }

    const wifi_param_meta_t* param = find_wifi_param(param_name);
    if (!param) {
        return ESP_ERR_NOT_FOUND;
    }

    info->name = param->name;
    info->type = param->type;
    info->offset = param->offset;
    info->size = param->size;

    if (param->type == CONFIG_TYPE_STRING) {
        info->constraints.string.max_length = param->constraints.string.max_length;
    } else {
        info->constraints.uint_range.min = param->constraints.uint_range.min;
        info->constraints.uint_range.max = param->constraints.uint_range.max;
    }

    return ESP_OK;
}

esp_err_t config_domain_wifi_get_uint32(
    const wifi_config_namespace_t* config,
    const char* param_name,
    uint32_t* value
) {
    if (!config || !param_name || !value) {
        return ESP_ERR_INVALID_ARG;
    }

    const wifi_param_meta_t* param = find_wifi_param(param_name);
    if (!param || param->type != CONFIG_TYPE_UINT32) {
        return ESP_ERR_NOT_FOUND;
    }

    *value = *(const uint32_t*)get_const_param_ptr(config, param);
    return ESP_OK;
}

esp_err_t config_domain_wifi_set_uint32(
    wifi_config_namespace_t* config,
    const char* param_name,
    uint32_t value
) {
    if (!config || !param_name) {
        return ESP_ERR_INVALID_ARG;
    }

    const wifi_param_meta_t* param = find_wifi_param(param_name);
    if (!param || param->type != CONFIG_TYPE_UINT32) {
        return ESP_ERR_NOT_FOUND;
    }

    if (value < param->constraints.uint_range.min || value > param->constraints.uint_range.max) {
        return ESP_ERR_INVALID_ARG;
    }

    *(uint32_t*)get_param_ptr(config, param) = value;
    return ESP_OK;
}

esp_err_t config_domain_wifi_get_string(
    const wifi_config_namespace_t* config,
    const char* param_name,
    char* value,
    size_t max_len
) {
    if (!config || !param_name || !value || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const wifi_param_meta_t* param = find_wifi_param(param_name);
    if (!param || param->type != CONFIG_TYPE_STRING) {
        return ESP_ERR_NOT_FOUND;
    }

    const char* src = (const char*)get_const_param_ptr(config, param);
    strncpy(value, src, max_len - 1);
    value[max_len - 1] = '\0';

    return ESP_OK;
}

esp_err_t config_domain_wifi_set_string(
    wifi_config_namespace_t* config,
    const char* param_name,
    const char* value
) {
    if (!config || !param_name || !value) {
        return ESP_ERR_INVALID_ARG;
    }

    const wifi_param_meta_t* param = find_wifi_param(param_name);
    if (!param || param->type != CONFIG_TYPE_STRING) {
        return ESP_ERR_NOT_FOUND;
    }

    size_t value_len = strlen(value);
    if (value_len >= param->constraints.string.max_length) {
        return ESP_ERR_INVALID_ARG;
    }

    char* dst = (char*)get_param_ptr(config, param);
    strncpy(dst, value, param->size - 1);
    dst[param->size - 1] = '\0';

    return ESP_OK;
}
