/*
 * System namespace descriptor and context.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "config_manager_core_types.h"
#include "config_ns_system_api.h"
#include "config_namespace_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nvs_handle_t* nvs_handle;
    bool* namespace_dirty;
    bool* namespace_loaded;
    system_config_t* config;
    uint16_t schema_version;
    controller_driver_type_e fallback_controller_type;
} config_system_namespace_context_t;

extern const config_namespace_descriptor_t g_system_namespace_descriptor;

void config_system_namespace_bind(
    nvs_handle_t* nvs_handle,
    bool* namespace_dirty,
    bool* namespace_loaded,
    uint16_t schema_version,
    controller_driver_type_e fallback_controller_type
);

void* config_system_namespace_context(void);
system_config_t* config_system_namespace_config(void);

#ifdef __cplusplus
}
#endif
