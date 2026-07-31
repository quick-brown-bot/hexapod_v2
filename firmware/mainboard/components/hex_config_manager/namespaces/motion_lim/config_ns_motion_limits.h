/*
 * Motion limits namespace descriptor and context.
 */

#pragma once

#include <stdbool.h>

#include "config_manager_core_types.h"
#include "config_ns_motion_limits_api.h"
#include "config_namespace_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nvs_handle_t* nvs_handle;
    bool* namespace_dirty;
    bool* namespace_loaded;
    motion_limits_config_t* config;
} config_motion_limits_namespace_context_t;

extern const config_namespace_descriptor_t g_motion_limits_namespace_descriptor;

void config_motion_limits_namespace_bind(
    nvs_handle_t* nvs_handle,
    bool* namespace_dirty,
    bool* namespace_loaded
);

void* config_motion_limits_namespace_context(void);
motion_limits_config_t* config_motion_limits_namespace_config(void);

#ifdef __cplusplus
}
#endif
