/*
 * Generic typed parameter and discovery APIs.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "config_manager_core_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t hexapod_config_get_bool(const char* namespace_str, const char* param_name, bool* value);
esp_err_t hexapod_config_set_bool(const char* namespace_str, const char* param_name, bool value, bool persist);

esp_err_t hexapod_config_get_int32(const char* namespace_str, const char* param_name, int32_t* value);
esp_err_t hexapod_config_set_int32(const char* namespace_str, const char* param_name, int32_t value, bool persist);

esp_err_t hexapod_config_get_uint32(const char* namespace_str, const char* param_name, uint32_t* value);
esp_err_t hexapod_config_set_uint32(const char* namespace_str, const char* param_name, uint32_t value, bool persist);

esp_err_t hexapod_config_get_float(const char* namespace_str, const char* param_name, float* value);
esp_err_t hexapod_config_set_float(const char* namespace_str, const char* param_name, float value, bool persist);

esp_err_t hexapod_config_get_string(const char* namespace_str, const char* param_name, char* value, size_t max_len);
esp_err_t hexapod_config_set_string(const char* namespace_str, const char* param_name, const char* value, bool persist);

esp_err_t config_list_namespaces(const char** namespace_names, size_t* count);
esp_err_t config_list_parameters(const char* namespace_str, const char** param_names,
                                size_t max_params, size_t* count);
esp_err_t config_get_parameter_info(const char* namespace_str, const char* param_name,
                                   config_param_info_t* info);

#ifdef __cplusplus
}
#endif
