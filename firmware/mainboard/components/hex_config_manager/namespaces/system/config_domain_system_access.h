/*
 * System domain parameter access helpers.
 *
 * Encapsulates system namespace metadata and typed get/set logic.
 */

#pragma once

#include <stddef.h>

#include "config_manager_core_types.h"
#include "config_ns_system_api.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t config_domain_system_list_parameters(
    const char** param_names,
    size_t max_params,
    size_t* count
);

esp_err_t config_domain_system_get_param_info(
    const char* param_name,
    config_param_info_t* info
);

esp_err_t config_domain_system_get_raw(
    const system_config_t* config,
    const char* param_name,
    void* value_out,
    size_t value_size
);

esp_err_t config_domain_system_get_bool(
    const system_config_t* config,
    const char* param_name,
    bool* value
);

esp_err_t config_domain_system_set_bool(
    system_config_t* config,
    const char* param_name,
    bool value
);

esp_err_t config_domain_system_get_int32(
    const system_config_t* config,
    const char* param_name,
    int32_t* value
);

esp_err_t config_domain_system_set_int32(
    system_config_t* config,
    const char* param_name,
    int32_t value
);

esp_err_t config_domain_system_get_uint32(
    const system_config_t* config,
    const char* param_name,
    uint32_t* value
);

esp_err_t config_domain_system_set_uint32(
    system_config_t* config,
    const char* param_name,
    uint32_t value
);

esp_err_t config_domain_system_get_float(
    const system_config_t* config,
    const char* param_name,
    float* value
);

esp_err_t config_domain_system_set_float(
    system_config_t* config,
    const char* param_name,
    float value
);

esp_err_t config_domain_system_get_string(
    const system_config_t* config,
    const char* param_name,
    char* value,
    size_t max_len
);

esp_err_t config_domain_system_set_string(
    system_config_t* config,
    const char* param_name,
    const char* value
);

#ifdef __cplusplus
}
#endif
