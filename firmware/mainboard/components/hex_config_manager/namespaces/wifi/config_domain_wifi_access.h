/*
 * WiFi domain parameter access helpers.
 *
 * Encapsulates wifi namespace metadata and typed get/set logic.
 */

#pragma once

#include <stddef.h>

#include "config_manager_core_types.h"
#include "config_ns_wifi_api.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t config_domain_wifi_list_parameters(
    const char** param_names,
    size_t max_params,
    size_t* count
);

esp_err_t config_domain_wifi_get_param_info(
    const char* param_name,
    config_param_info_t* info
);

esp_err_t config_domain_wifi_get_uint32(
    const wifi_config_namespace_t* config,
    const char* param_name,
    uint32_t* value
);

esp_err_t config_domain_wifi_set_uint32(
    wifi_config_namespace_t* config,
    const char* param_name,
    uint32_t value
);

esp_err_t config_domain_wifi_get_string(
    const wifi_config_namespace_t* config,
    const char* param_name,
    char* value,
    size_t max_len
);

esp_err_t config_domain_wifi_set_string(
    wifi_config_namespace_t* config,
    const char* param_name,
    const char* value
);

#ifdef __cplusplus
}
#endif
