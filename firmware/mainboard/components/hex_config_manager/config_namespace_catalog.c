#include "config_namespace_catalog.h"

#include "config_namespace_registry.h"
#include "config_ns_controller.h"
#include "config_ns_gait.h"
#include "config_ns_joint_calib.h"
#include "config_ns_leg_geometry.h"
#include "config_ns_motion_limits.h"
#include "config_ns_servo_map.h"
#include "config_ns_system.h"
#include "config_ns_wifi.h"

#include <stddef.h>

typedef struct {
    const config_namespace_descriptor_t* descriptor;
    void* context;
} namespace_registration_entry_t;

esp_err_t config_namespace_catalog_register_all(
    config_manager_state_t* state,
    uint16_t schema_version,
    controller_driver_type_e fallback_controller_type
) {
    if (!state) {
        return ESP_ERR_INVALID_ARG;
    }

    config_system_namespace_bind(
        &state->nvs_handles[CONFIG_NS_SYSTEM],
        &state->namespace_dirty[CONFIG_NS_SYSTEM],
        &state->namespace_loaded[CONFIG_NS_SYSTEM],
        schema_version,
        fallback_controller_type
    );
    config_joint_namespace_bind(
        &state->nvs_handles[CONFIG_NS_JOINT_CALIB],
        &state->namespace_dirty[CONFIG_NS_JOINT_CALIB],
        &state->namespace_loaded[CONFIG_NS_JOINT_CALIB]
    );
    config_leg_geometry_namespace_bind(
        &state->nvs_handles[CONFIG_NS_LEG_GEOMETRY],
        &state->namespace_dirty[CONFIG_NS_LEG_GEOMETRY],
        &state->namespace_loaded[CONFIG_NS_LEG_GEOMETRY]
    );
    config_motion_limits_namespace_bind(
        &state->nvs_handles[CONFIG_NS_MOTION_LIMITS],
        &state->namespace_dirty[CONFIG_NS_MOTION_LIMITS],
        &state->namespace_loaded[CONFIG_NS_MOTION_LIMITS]
    );
    config_controller_namespace_bind(
        &state->nvs_handles[CONFIG_NS_CONTROLLER],
        &state->namespace_dirty[CONFIG_NS_CONTROLLER],
        &state->namespace_loaded[CONFIG_NS_CONTROLLER]
    );
    config_wifi_namespace_bind(
        &state->nvs_handles[CONFIG_NS_WIFI],
        &state->namespace_dirty[CONFIG_NS_WIFI],
        &state->namespace_loaded[CONFIG_NS_WIFI]
    );
    config_servo_map_namespace_bind(
        &state->nvs_handles[CONFIG_NS_SERVO_MAP],
        &state->namespace_dirty[CONFIG_NS_SERVO_MAP],
        &state->namespace_loaded[CONFIG_NS_SERVO_MAP]
    );
    config_gait_namespace_bind(
        &state->nvs_handles[CONFIG_NS_GAIT],
        &state->namespace_dirty[CONFIG_NS_GAIT],
        &state->namespace_loaded[CONFIG_NS_GAIT]
    );

    namespace_registration_entry_t namespace_registration_entries[] = {
        { &g_system_namespace_descriptor, config_system_namespace_context() },
        { &g_joint_namespace_descriptor, config_joint_namespace_context() },
        { &g_leg_geometry_namespace_descriptor, config_leg_geometry_namespace_context() },
        { &g_motion_limits_namespace_descriptor, config_motion_limits_namespace_context() },
        { &g_controller_namespace_descriptor, config_controller_namespace_context() },
        { &g_wifi_namespace_descriptor, config_wifi_namespace_context() },
        { &g_servo_map_namespace_descriptor, config_servo_map_namespace_context() },
        { &g_gait_namespace_descriptor, config_gait_namespace_context() },
    };

    config_namespace_registry_reset();

    size_t descriptor_count = sizeof(namespace_registration_entries) / sizeof(namespace_registration_entries[0]);
    for (size_t i = 0; i < descriptor_count; i++) {
        const config_namespace_descriptor_t* descriptor = namespace_registration_entries[i].descriptor;
        void* context = namespace_registration_entries[i].context;
        esp_err_t err = config_namespace_registry_register(descriptor, context);
        if (err != ESP_OK) {
            return err;
        }
    }

    return ESP_OK;
}

system_config_t* config_namespace_catalog_system_config(void) {
    return config_system_namespace_config();
}

joint_calib_config_t* config_namespace_catalog_joint_calib_config(void) {
    return config_joint_namespace_config();
}

leg_geometry_config_t* config_namespace_catalog_leg_geometry_config(void) {
    return config_leg_geometry_namespace_config();
}

motion_limits_config_t* config_namespace_catalog_motion_limits_config(void) {
    return config_motion_limits_namespace_config();
}

controller_config_namespace_t* config_namespace_catalog_controller_config(void) {
    return config_controller_namespace_config();
}

wifi_config_namespace_t* config_namespace_catalog_wifi_config(void) {
    return config_wifi_namespace_config();
}

servo_map_config_t* config_namespace_catalog_servo_map_config(void) {
    return config_servo_map_namespace_config();
}

gait_config_t* config_namespace_catalog_gait_config(void) {
    return config_gait_namespace_config();
}
