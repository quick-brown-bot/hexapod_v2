/*
 * Controller namespace descriptor and context.
 */

#pragma once

#include <stdbool.h>

#include "config_manager_core_types.h"
#include "config_ns_controller_api.h"
#include "config_namespace_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nvs_handle_t* nvs_handle;
    bool* namespace_dirty;
    bool* namespace_loaded;
    controller_config_namespace_t* config;
} config_controller_namespace_context_t;

extern const config_namespace_descriptor_t g_controller_namespace_descriptor;

void config_controller_namespace_bind(
    nvs_handle_t* nvs_handle,
    bool* namespace_dirty,
    bool* namespace_loaded
);

void* config_controller_namespace_context(void);
controller_config_namespace_t* config_controller_namespace_config(void);

#ifdef __cplusplus
}
#endif
