/*
 * Domain persistence helpers for NVS.
 *
 * Isolates namespace default initialization writes from migration orchestration.
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "nvs.h"
#include "config_ns_controller_api.h"
#include "config_ns_joint_api.h"
#include "config_ns_leg_geometry_api.h"
#include "config_ns_motion_limits_api.h"
#include "config_ns_gait_api.h"
#include "config_ns_servo_map_api.h"
#include "config_ns_system_api.h"
#include "config_ns_wifi_api.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t config_domain_system_write_defaults_to_nvs(
    nvs_handle_t handle,
    uint16_t schema_version
);

esp_err_t config_domain_joint_cal_write_defaults_to_nvs(nvs_handle_t handle);
esp_err_t config_domain_leg_geometry_write_defaults_to_nvs(nvs_handle_t handle);
esp_err_t config_domain_motion_limits_write_defaults_to_nvs(nvs_handle_t handle);
esp_err_t config_domain_gait_write_defaults_to_nvs(nvs_handle_t handle);
esp_err_t config_domain_controller_write_defaults_to_nvs(nvs_handle_t handle);
esp_err_t config_domain_wifi_write_defaults_to_nvs(nvs_handle_t handle);
esp_err_t config_domain_servo_map_write_defaults_to_nvs(nvs_handle_t handle);

esp_err_t config_domain_system_load_from_nvs(
    nvs_handle_t handle,
    system_config_t* config,
    uint16_t fallback_schema_version,
    controller_driver_type_e fallback_controller_type
);

esp_err_t config_domain_joint_cal_load_from_nvs(
    nvs_handle_t handle,
    joint_calib_config_t* config
);

esp_err_t config_domain_leg_geometry_load_from_nvs(
    nvs_handle_t handle,
    leg_geometry_config_t* config
);

esp_err_t config_domain_motion_limits_load_from_nvs(
    nvs_handle_t handle,
    motion_limits_config_t* config
);

esp_err_t config_domain_gait_load_from_nvs(
    nvs_handle_t handle,
    gait_config_t* config
);

esp_err_t config_domain_controller_load_from_nvs(
    nvs_handle_t handle,
    controller_config_namespace_t* config
);

esp_err_t config_domain_wifi_load_from_nvs(
    nvs_handle_t handle,
    wifi_config_namespace_t* config
);

esp_err_t config_domain_servo_map_load_from_nvs(
    nvs_handle_t handle,
    servo_map_config_t* config
);

esp_err_t config_domain_system_save_to_nvs(
    nvs_handle_t handle,
    const system_config_t* config
);

esp_err_t config_domain_joint_cal_save_to_nvs(
    nvs_handle_t handle,
    const joint_calib_config_t* config
);

esp_err_t config_domain_leg_geometry_save_to_nvs(
    nvs_handle_t handle,
    const leg_geometry_config_t* config
);

esp_err_t config_domain_motion_limits_save_to_nvs(
    nvs_handle_t handle,
    const motion_limits_config_t* config
);

esp_err_t config_domain_gait_save_to_nvs(
    nvs_handle_t handle,
    const gait_config_t* config
);

esp_err_t config_domain_controller_save_to_nvs(
    nvs_handle_t handle,
    const controller_config_namespace_t* config
);

esp_err_t config_domain_wifi_save_to_nvs(
    nvs_handle_t handle,
    const wifi_config_namespace_t* config
);

esp_err_t config_domain_servo_map_save_to_nvs(
    nvs_handle_t handle,
    const servo_map_config_t* config
);

#ifdef __cplusplus
}
#endif
