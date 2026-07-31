/*
 * WiFi namespace descriptor and context.
 */

#pragma once

#include <stdbool.h>

#include "config_manager_core_types.h"
#include "config_ns_wifi_api.h"
#include "config_namespace_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nvs_handle_t* nvs_handle;
    bool* namespace_dirty;
    bool* namespace_loaded;
    wifi_config_namespace_t* config;
} config_wifi_namespace_context_t;

extern const config_namespace_descriptor_t g_wifi_namespace_descriptor;

void config_wifi_namespace_bind(
    nvs_handle_t* nvs_handle,
    bool* namespace_dirty,
    bool* namespace_loaded
);

void* config_wifi_namespace_context(void);
wifi_config_namespace_t* config_wifi_namespace_config(void);

#ifdef __cplusplus
}
#endif
