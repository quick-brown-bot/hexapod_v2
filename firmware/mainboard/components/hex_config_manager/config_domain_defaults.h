/*
 * Domain defaults for configuration namespaces.
 *
 * Centralizes in-memory default population for system and joint calibration
 * domains and exposes a reusable joint-default helper.
 */

#pragma once

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

void config_domain_fill_joint_default(int joint, joint_calib_t* calib);

void config_load_system_defaults(system_config_t* config);
void config_load_joint_calib_defaults(joint_calib_config_t* config);
void config_load_leg_geometry_defaults(leg_geometry_config_t* config);
void config_load_motion_limits_defaults(motion_limits_config_t* config);
void config_load_gait_defaults(gait_config_t* config);
void config_load_controller_defaults(controller_config_namespace_t* config);
void config_load_wifi_defaults(wifi_config_namespace_t* config);
void config_load_servo_map_defaults(servo_map_config_t* config);

#ifdef __cplusplus
}
#endif
