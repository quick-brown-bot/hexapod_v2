/*
 * Servo map namespace descriptor and context.
 */

#pragma once

#include <stdbool.h>

#include "config_manager_core_types.h"
#include "config_namespace_registry.h"
#include "config_ns_servo_map_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nvs_handle_t* nvs_handle;
    bool* namespace_dirty;
    bool* namespace_loaded;
    servo_map_config_t* config;
} config_servo_map_namespace_context_t;

extern const config_namespace_descriptor_t g_servo_map_namespace_descriptor;

void config_servo_map_namespace_bind(
    nvs_handle_t* nvs_handle,
    bool* namespace_dirty,
    bool* namespace_loaded
);

void* config_servo_map_namespace_context(void);
servo_map_config_t* config_servo_map_namespace_config(void);

#ifdef __cplusplus
}
#endif
