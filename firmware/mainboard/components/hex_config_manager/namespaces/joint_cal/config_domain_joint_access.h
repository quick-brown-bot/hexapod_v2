/*
 * Joint calibration domain parameter access helpers.
 *
 * Encapsulates parsing and typed get/set logic for joint_cal namespace parameters.
 */

#pragma once

#include "config_manager_core_types.h"
#include "config_ns_joint_api.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t config_domain_joint_get_int32(
    const joint_calib_config_t* config,
    const char* param_name,
    int32_t* value
);

esp_err_t config_domain_joint_set_int32(
    joint_calib_config_t* config,
    const char* param_name,
    int32_t value
);

esp_err_t config_domain_joint_get_float(
    const joint_calib_config_t* config,
    const char* param_name,
    float* value
);

esp_err_t config_domain_joint_set_float(
    joint_calib_config_t* config,
    const char* param_name,
    float value
);

esp_err_t config_domain_joint_parse_param_name(
    const char* param_name,
    int* leg_index,
    int* joint_index,
    const char** param_suffix
);

#ifdef __cplusplus
}
#endif
