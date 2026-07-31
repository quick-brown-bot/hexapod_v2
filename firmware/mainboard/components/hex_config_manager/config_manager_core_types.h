/*
 * Core config manager shared types.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nvs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CONFIG_NS_SYSTEM = 0,
    CONFIG_NS_JOINT_CALIB = 1,
    CONFIG_NS_LEG_GEOMETRY = 2,
    CONFIG_NS_MOTION_LIMITS = 3,
    CONFIG_NS_CONTROLLER = 4,
    CONFIG_NS_WIFI = 5,
    CONFIG_NS_SERVO_MAP = 6,
    CONFIG_NS_GAIT = 7,
    CONFIG_NS_COUNT
} config_namespace_t;

typedef struct {
    bool namespace_dirty[CONFIG_NS_COUNT];
    bool namespace_loaded[CONFIG_NS_COUNT];
    bool initialized;
    nvs_handle_t nvs_handles[CONFIG_NS_COUNT];
} config_manager_state_t;

typedef enum {
    CONFIG_TYPE_BOOL = 0,
    CONFIG_TYPE_INT32,
    CONFIG_TYPE_UINT16,
    CONFIG_TYPE_UINT32,
    CONFIG_TYPE_FLOAT,
    CONFIG_TYPE_STRING,
    CONFIG_TYPE_COUNT
} config_param_type_t;

typedef struct {
    const char* name;
    config_param_type_t type;
    size_t offset;
    size_t size;
    union {
        struct { int32_t min, max; } int_range;
        struct { uint32_t min, max; } uint_range;
        struct { float min, max; } float_range;
        struct { size_t max_length; } string;
    } constraints;
} config_param_info_t;

#ifdef __cplusplus
}
#endif
