/*
 * Configuration registry helpers.
 *
 * Encapsulates joint-calibration parameter metadata and discovery helpers
 * away from config manager runtime and persistence logic.
 */

#pragma once

#include <stddef.h>
#include "esp_err.h"
#include "config_manager_core_types.h"
#include "config_ns_joint_api.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t config_registry_list_joint_parameters(
    const char** param_names,
    size_t max_params,
    size_t* count
);

esp_err_t config_registry_build_joint_param_info(
    int leg_index,
    int joint_index,
    const char* param_suffix,
    const char* canonical_param_name,
    config_param_info_t* info
);

#ifdef __cplusplus
}
#endif
