/*
 * Gait namespace descriptor and context.
 */

#pragma once

#include <stdbool.h>

#include "config_manager_core_types.h"
#include "config_namespace_registry.h"
#include "config_ns_gait_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nvs_handle_t* nvs_handle;
    bool* namespace_dirty;
    bool* namespace_loaded;
    gait_config_t* config;
} config_gait_namespace_context_t;

extern const config_namespace_descriptor_t g_gait_namespace_descriptor;

void config_gait_namespace_bind(
    nvs_handle_t* nvs_handle,
    bool* namespace_dirty,
    bool* namespace_loaded
);

void* config_gait_namespace_context(void);
gait_config_t* config_gait_namespace_config(void);

#ifdef __cplusplus
}
#endif
