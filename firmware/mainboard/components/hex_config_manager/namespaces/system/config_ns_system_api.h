/*
 * Public system namespace types and APIs.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "controller_driver_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool emergency_stop_enabled;
    uint32_t auto_disarm_timeout;
    float safety_voltage_min;
    float temperature_limit_max;
    uint32_t motion_timeout_ms;
    uint32_t startup_delay_ms;
    uint32_t max_control_frequency;

    char robot_id[32];
    char robot_name[64];
    uint16_t config_version;
    controller_driver_type_e controller_type;
} system_config_t;

const system_config_t* config_get_system(void);
esp_err_t config_set_system(const system_config_t* config);

void config_load_system_defaults(system_config_t* config);

#ifdef __cplusplus
}
#endif
