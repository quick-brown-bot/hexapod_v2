/*
 * Namespace catalog for config manager.
 *
 * Centralizes namespace cache/context wiring and descriptor registration.
 */

#pragma once

#include <stdint.h>

#include "config_manager_core_types.h"
#include "config_ns_controller_api.h"
#include "config_ns_gait_api.h"
#include "config_ns_joint_api.h"
#include "config_ns_leg_geometry_api.h"
#include "config_ns_motion_limits_api.h"
#include "config_ns_servo_map_api.h"
#include "config_ns_system_api.h"
#include "config_ns_wifi_api.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t config_namespace_catalog_register_all(
    config_manager_state_t* state,
    uint16_t schema_version,
    controller_driver_type_e fallback_controller_type
);

system_config_t* config_namespace_catalog_system_config(void);
joint_calib_config_t* config_namespace_catalog_joint_calib_config(void);
leg_geometry_config_t* config_namespace_catalog_leg_geometry_config(void);
motion_limits_config_t* config_namespace_catalog_motion_limits_config(void);
controller_config_namespace_t* config_namespace_catalog_controller_config(void);
wifi_config_namespace_t* config_namespace_catalog_wifi_config(void);
servo_map_config_t* config_namespace_catalog_servo_map_config(void);
gait_config_t* config_namespace_catalog_gait_config(void);

#ifdef __cplusplus
}
#endif
