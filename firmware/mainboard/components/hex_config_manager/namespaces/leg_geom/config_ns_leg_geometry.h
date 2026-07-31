/*
 * Leg geometry namespace descriptor and context.
 */

#pragma once

#include <stdbool.h>

#include "config_manager_core_types.h"
#include "config_ns_leg_geometry_api.h"
#include "config_namespace_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nvs_handle_t* nvs_handle;
    bool* namespace_dirty;
    bool* namespace_loaded;
    leg_geometry_config_t* config;
} config_leg_geometry_namespace_context_t;

extern const config_namespace_descriptor_t g_leg_geometry_namespace_descriptor;

void config_leg_geometry_namespace_bind(
    nvs_handle_t* nvs_handle,
    bool* namespace_dirty,
    bool* namespace_loaded
);

void* config_leg_geometry_namespace_context(void);
leg_geometry_config_t* config_leg_geometry_namespace_config(void);

#ifdef __cplusplus
}
#endif
