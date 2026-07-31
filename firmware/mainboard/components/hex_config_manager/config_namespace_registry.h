/*
 * Namespace descriptor registry.
 *
 * Provides registration and lookup for namespace-specific handlers so
 * config API routing can stay registration-driven instead of switch-based.
 */

#pragma once

#include <stddef.h>

#include "config_manager_core_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    config_namespace_t ns_id;
    const char* ns_name;

    esp_err_t (*load_defaults)(void* ctx);
    esp_err_t (*load_from_nvs)(void* ctx);
    esp_err_t (*write_defaults_to_nvs)(void* ctx);
    esp_err_t (*save)(void* ctx);
    esp_err_t (*list_parameters)(void* ctx, const char** param_names, size_t max_params, size_t* count);
    esp_err_t (*get_parameter_info)(void* ctx, const char* param_name, config_param_info_t* info);
    esp_err_t (*get_raw)(void* ctx, const char* key, void* value_out, size_t value_size);

    esp_err_t (*get_bool)(void* ctx, const char* param_name, bool* value);
    esp_err_t (*set_bool)(void* ctx, const char* param_name, bool value);
    esp_err_t (*get_int32)(void* ctx, const char* param_name, int32_t* value);
    esp_err_t (*set_int32)(void* ctx, const char* param_name, int32_t value);
    esp_err_t (*get_uint32)(void* ctx, const char* param_name, uint32_t* value);
    esp_err_t (*set_uint32)(void* ctx, const char* param_name, uint32_t value);
    esp_err_t (*get_float)(void* ctx, const char* param_name, float* value);
    esp_err_t (*set_float)(void* ctx, const char* param_name, float value);
    esp_err_t (*get_string)(void* ctx, const char* param_name, char* value, size_t max_len);
    esp_err_t (*set_string)(void* ctx, const char* param_name, const char* value);
} config_namespace_descriptor_t;

typedef struct {
    const config_namespace_descriptor_t* descriptor;
    void* context;
} config_namespace_registration_t;

void config_namespace_registry_reset(void);
esp_err_t config_namespace_registry_register(const config_namespace_descriptor_t* descriptor, void* context);

const config_namespace_registration_t* config_namespace_registry_find_by_id(config_namespace_t ns_id);
const config_namespace_registration_t* config_namespace_registry_find_by_name(const char* ns_name);
size_t config_namespace_registry_count(void);
const config_namespace_registration_t* config_namespace_registry_get_at(size_t index);

#ifdef __cplusplus
}
#endif
