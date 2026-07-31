#include "config_registry.h"

#include <stdio.h>
#include <string.h>

// Joint calibration parameter metadata for a single joint.
typedef struct {
    const char* param_suffix;
    config_param_type_t type;
    size_t offset;
    size_t size;
    union {
        struct { int32_t min, max; } int_range;
        struct { float min, max; } float_range;
    } constraints;
} joint_param_meta_t;

static const joint_param_meta_t g_joint_param_table[] = {
    {
        .param_suffix = "offset",
        .type = CONFIG_TYPE_FLOAT,
        .offset = offsetof(joint_calib_t, zero_offset_rad),
        .size = sizeof(float),
        .constraints = { .float_range = { -6.28f, 6.28f } }
    },
    {
        .param_suffix = "invert",
        .type = CONFIG_TYPE_INT32,
        .offset = offsetof(joint_calib_t, invert_sign),
        .size = sizeof(int8_t),
        .constraints = { .int_range = { -1, 1 } }
    },
    {
        .param_suffix = "min",
        .type = CONFIG_TYPE_FLOAT,
        .offset = offsetof(joint_calib_t, min_rad),
        .size = sizeof(float),
        .constraints = { .float_range = { -6.28f, 6.28f } }
    },
    {
        .param_suffix = "max",
        .type = CONFIG_TYPE_FLOAT,
        .offset = offsetof(joint_calib_t, max_rad),
        .size = sizeof(float),
        .constraints = { .float_range = { -6.28f, 6.28f } }
    },
    {
        .param_suffix = "pwm_min",
        .type = CONFIG_TYPE_INT32,
        .offset = offsetof(joint_calib_t, pwm_min_us),
        .size = sizeof(int32_t),
        .constraints = { .int_range = { 500, 3000 } }
    },
    {
        .param_suffix = "pwm_max",
        .type = CONFIG_TYPE_INT32,
        .offset = offsetof(joint_calib_t, pwm_max_us),
        .size = sizeof(int32_t),
        .constraints = { .int_range = { 500, 3000 } }
    },
    {
        .param_suffix = "neutral",
        .type = CONFIG_TYPE_INT32,
        .offset = offsetof(joint_calib_t, neutral_us),
        .size = sizeof(int32_t),
        .constraints = { .int_range = { 500, 3000 } }
    }
};

static const char* g_joint_full_names[] = { "coxa", "femur", "tibia" };

#define JOINT_PARAM_COUNT (sizeof(g_joint_param_table) / sizeof(g_joint_param_table[0]))
#define JOINT_PARAM_NAME_CAPACITY (NUM_LEGS * NUM_JOINTS_PER_LEG * JOINT_PARAM_COUNT)

static char g_joint_param_names[JOINT_PARAM_NAME_CAPACITY][32];

static const joint_param_meta_t* find_joint_param_meta(const char* param_suffix) {
    for (size_t i = 0; i < JOINT_PARAM_COUNT; i++) {
        if (strcmp(g_joint_param_table[i].param_suffix, param_suffix) == 0) {
            return &g_joint_param_table[i];
        }
    }
    return NULL;
}

esp_err_t config_registry_list_joint_parameters(
    const char** param_names,
    size_t max_params,
    size_t* count
) {
    if (!param_names || !count) {
        return ESP_ERR_INVALID_ARG;
    }

    const size_t total = NUM_LEGS * NUM_JOINTS_PER_LEG * JOINT_PARAM_COUNT;
    size_t to_emit = total;
    if (to_emit > max_params) {
        to_emit = max_params;
    }

    size_t idx = 0;
    for (int leg = 0; leg < NUM_LEGS && idx < to_emit; leg++) {
        for (size_t joint = 0; joint < NUM_JOINTS_PER_LEG && idx < to_emit; joint++) {
            for (size_t meta = 0; meta < JOINT_PARAM_COUNT && idx < to_emit; meta++) {
                snprintf(
                    g_joint_param_names[idx],
                    sizeof(g_joint_param_names[0]),
                    "leg%d_%s_%s",
                    leg,
                    g_joint_full_names[joint],
                    g_joint_param_table[meta].param_suffix
                );
                param_names[idx] = g_joint_param_names[idx];
                idx++;
            }
        }
    }

    *count = idx;
    return ESP_OK;
}

esp_err_t config_registry_build_joint_param_info(
    int leg_index,
    int joint_index,
    const char* param_suffix,
    const char* canonical_param_name,
    config_param_info_t* info
) {
    if (!param_suffix || !canonical_param_name || !info) {
        return ESP_ERR_INVALID_ARG;
    }
    if (leg_index < 0 || leg_index >= NUM_LEGS || joint_index < 0 || joint_index >= NUM_JOINTS_PER_LEG) {
        return ESP_ERR_INVALID_ARG;
    }

    const joint_param_meta_t* meta = find_joint_param_meta(param_suffix);
    if (!meta) {
        return ESP_ERR_NOT_FOUND;
    }

    info->name = canonical_param_name;
    info->type = meta->type;
    info->offset = meta->offset;
    info->size = meta->size;

    switch (meta->type) {
        case CONFIG_TYPE_INT32:
            info->constraints.int_range.min = meta->constraints.int_range.min;
            info->constraints.int_range.max = meta->constraints.int_range.max;
            break;
        case CONFIG_TYPE_FLOAT:
            info->constraints.float_range.min = meta->constraints.float_range.min;
            info->constraints.float_range.max = meta->constraints.float_range.max;
            break;
        default:
            break;
    }

    return ESP_OK;
}
