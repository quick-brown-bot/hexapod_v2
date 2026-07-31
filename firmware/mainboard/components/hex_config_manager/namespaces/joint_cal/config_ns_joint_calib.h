/*
 * Joint calibration namespace descriptor and context.
 */

#pragma once

#include <stdbool.h>

#include "config_manager_core_types.h"
#include "config_ns_joint_api.h"
#include "config_namespace_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nvs_handle_t* nvs_handle;
    bool* namespace_dirty;
    bool* namespace_loaded;
    joint_calib_config_t* config;
} config_joint_calib_namespace_context_t;

extern const config_namespace_descriptor_t g_joint_namespace_descriptor;

void config_joint_namespace_bind(
    nvs_handle_t* nvs_handle,
    bool* namespace_dirty,
    bool* namespace_loaded
);

void* config_joint_namespace_context(void);
joint_calib_config_t* config_joint_namespace_config(void);

#ifdef __cplusplus
}
#endif
