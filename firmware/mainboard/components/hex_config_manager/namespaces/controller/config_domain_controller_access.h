/*
 * Controller domain parameter access helpers.
 *
 * Encapsulates controller namespace metadata and typed get/set logic.
 */

#pragma once

#include <stddef.h>

#include "config_manager_core_types.h"
#include "config_ns_controller_api.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t config_domain_controller_list_parameters(
    const char** param_names,
    size_t max_params,
    size_t* count
);

esp_err_t config_domain_controller_get_param_info(
    const char* param_name,
    config_param_info_t* info
);

esp_err_t config_domain_controller_get_int32(
    const controller_config_namespace_t* config,
    const char* param_name,
    int32_t* value
);

esp_err_t config_domain_controller_set_int32(
    controller_config_namespace_t* config,
    const char* param_name,
    int32_t value
);

esp_err_t config_domain_controller_get_uint32(
    const controller_config_namespace_t* config,
    const char* param_name,
    uint32_t* value
);

esp_err_t config_domain_controller_set_uint32(
    controller_config_namespace_t* config,
    const char* param_name,
    uint32_t value
);

esp_err_t config_domain_controller_get_float(
    const controller_config_namespace_t* config,
    const char* param_name,
    float* value
);

esp_err_t config_domain_controller_set_float(
    controller_config_namespace_t* config,
    const char* param_name,
    float value
);

esp_err_t config_domain_controller_get_bool(
    const controller_config_namespace_t* config,
    const char* param_name,
    bool* value
);

esp_err_t config_domain_controller_set_bool(
    controller_config_namespace_t* config,
    const char* param_name,
    bool value
);

#ifdef __cplusplus
}
#endif
