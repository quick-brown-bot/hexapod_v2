#include "config_ns_motion_limits.h"

#include <stddef.h>
#include <string.h>

#include "config_domain_defaults.h"
#include "config_manager_runtime_api.h"
#include "config_domain_persistence_nvs.h"

typedef struct {
    const char* name;
    size_t offset;
    float min_value;
    float max_value;
} motion_param_meta_t;

#define MOTION_ARRAY_OFFSET(field, idx) (offsetof(motion_limits_config_t, field) + ((size_t)(idx) * sizeof(float)))

static const motion_param_meta_t g_motion_param_table[] = {
    { "max_velocity_coxa", MOTION_ARRAY_OFFSET(max_velocity, 0), 0.0f, 20.0f },
    { "max_velocity_femur", MOTION_ARRAY_OFFSET(max_velocity, 1), 0.0f, 20.0f },
    { "max_velocity_tibia", MOTION_ARRAY_OFFSET(max_velocity, 2), 0.0f, 20.0f },

    { "max_acceleration_coxa", MOTION_ARRAY_OFFSET(max_acceleration, 0), 0.0f, 5000.0f },
    { "max_acceleration_femur", MOTION_ARRAY_OFFSET(max_acceleration, 1), 0.0f, 5000.0f },
    { "max_acceleration_tibia", MOTION_ARRAY_OFFSET(max_acceleration, 2), 0.0f, 5000.0f },

    { "max_jerk_coxa", MOTION_ARRAY_OFFSET(max_jerk, 0), 0.0f, 50000.0f },
    { "max_jerk_femur", MOTION_ARRAY_OFFSET(max_jerk, 1), 0.0f, 50000.0f },
    { "max_jerk_tibia", MOTION_ARRAY_OFFSET(max_jerk, 2), 0.0f, 50000.0f },

    { "velocity_filter_alpha", offsetof(motion_limits_config_t, velocity_filter_alpha), 0.0f, 1.0f },
    { "accel_filter_alpha", offsetof(motion_limits_config_t, accel_filter_alpha), 0.0f, 1.0f },
    { "leg_velocity_filter_alpha", offsetof(motion_limits_config_t, leg_velocity_filter_alpha), 0.0f, 1.0f },
    { "body_velocity_filter_alpha", offsetof(motion_limits_config_t, body_velocity_filter_alpha), 0.0f, 1.0f },
    { "body_pitch_filter_alpha", offsetof(motion_limits_config_t, body_pitch_filter_alpha), 0.0f, 1.0f },
    { "body_roll_filter_alpha", offsetof(motion_limits_config_t, body_roll_filter_alpha), 0.0f, 1.0f },

    { "front_to_back_distance", offsetof(motion_limits_config_t, front_to_back_distance), 0.0f, 2.0f },
    { "left_to_right_distance", offsetof(motion_limits_config_t, left_to_right_distance), 0.0f, 2.0f },
    { "max_leg_velocity", offsetof(motion_limits_config_t, max_leg_velocity), 0.0f, 20.0f },
    { "max_body_velocity", offsetof(motion_limits_config_t, max_body_velocity), 0.0f, 10.0f },
    { "max_angular_velocity", offsetof(motion_limits_config_t, max_angular_velocity), 0.0f, 20.0f },
    { "min_dt", offsetof(motion_limits_config_t, min_dt), 0.0001f, 1.0f },
    { "max_dt", offsetof(motion_limits_config_t, max_dt), 0.0001f, 1.0f },

    { "body_offset_x", offsetof(motion_limits_config_t, body_offset_x), -1.0f, 1.0f },
    { "body_offset_y", offsetof(motion_limits_config_t, body_offset_y), -1.0f, 1.0f },
    { "body_offset_z", offsetof(motion_limits_config_t, body_offset_z), -1.0f, 1.0f },
};

#define MOTION_PARAM_COUNT (sizeof(g_motion_param_table) / sizeof(g_motion_param_table[0]))

static motion_limits_config_t g_motion_limits_namespace_config = {0};
static config_motion_limits_namespace_context_t g_motion_limits_namespace_context = {0};

void config_motion_limits_namespace_bind(
    nvs_handle_t* nvs_handle,
    bool* namespace_dirty,
    bool* namespace_loaded
) {
    g_motion_limits_namespace_context.nvs_handle = nvs_handle;
    g_motion_limits_namespace_context.namespace_dirty = namespace_dirty;
    g_motion_limits_namespace_context.namespace_loaded = namespace_loaded;
    g_motion_limits_namespace_context.config = &g_motion_limits_namespace_config;
}

void* config_motion_limits_namespace_context(void) {
    return &g_motion_limits_namespace_context;
}

motion_limits_config_t* config_motion_limits_namespace_config(void) {
    return &g_motion_limits_namespace_config;
}

const motion_limits_config_t* config_get_motion_limits(void) {
    return &g_motion_limits_namespace_config;
}

esp_err_t config_set_motion_limits(const motion_limits_config_t* config) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    g_motion_limits_namespace_config = *config;
    if (g_motion_limits_namespace_context.namespace_dirty) {
        *g_motion_limits_namespace_context.namespace_dirty = true;
    }

    esp_err_t err = config_manager_save_namespace(CONFIG_NS_MOTION_LIMITS);
    if (err == ESP_OK && g_motion_limits_namespace_context.namespace_dirty) {
        *g_motion_limits_namespace_context.namespace_dirty = false;
    }

    return err;
}

static config_motion_limits_namespace_context_t* motion_ctx(void* ctx) {
    return (config_motion_limits_namespace_context_t*)ctx;
}

static const motion_param_meta_t* find_motion_param_meta(const char* param_name) {
    if (!param_name) {
        return NULL;
    }

    for (size_t i = 0; i < MOTION_PARAM_COUNT; i++) {
        if (strcmp(param_name, g_motion_param_table[i].name) == 0) {
            return &g_motion_param_table[i];
        }
    }

    return NULL;
}

static esp_err_t motion_ns_load_defaults(void* ctx) {
    config_motion_limits_namespace_context_t* c = motion_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    config_load_motion_limits_defaults(c->config);
    return ESP_OK;
}

static esp_err_t motion_ns_load_from_nvs(void* ctx) {
    config_motion_limits_namespace_context_t* c = motion_ctx(ctx);
    if (!c || !c->nvs_handle || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_domain_motion_limits_load_from_nvs(*c->nvs_handle, c->config);
    if (err != ESP_OK) {
        return err;
    }

    if (c->namespace_loaded) {
        *c->namespace_loaded = true;
    }
    if (c->namespace_dirty) {
        *c->namespace_dirty = false;
    }

    return ESP_OK;
}

static esp_err_t motion_ns_write_defaults_to_nvs(void* ctx) {
    config_motion_limits_namespace_context_t* c = motion_ctx(ctx);
    if (!c || !c->nvs_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_motion_limits_write_defaults_to_nvs(*c->nvs_handle);
}

static esp_err_t motion_ns_save(void* ctx) {
    config_motion_limits_namespace_context_t* c = motion_ctx(ctx);
    if (!c || !c->nvs_handle || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_domain_motion_limits_save_to_nvs(*c->nvs_handle, c->config);
    if (err != ESP_OK) {
        return err;
    }

    if (c->namespace_dirty) {
        *c->namespace_dirty = false;
    }

    return ESP_OK;
}

static esp_err_t motion_ns_list_parameters(void* ctx, const char** param_names, size_t max_params, size_t* count) {
    (void)ctx;

    if (!param_names || !count) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t to_emit = MOTION_PARAM_COUNT;
    if (to_emit > max_params) {
        to_emit = max_params;
    }

    for (size_t i = 0; i < to_emit; i++) {
        param_names[i] = g_motion_param_table[i].name;
    }

    *count = to_emit;
    return ESP_OK;
}

static esp_err_t motion_ns_get_param_info(void* ctx, const char* param_name, config_param_info_t* info) {
    (void)ctx;

    if (!param_name || !info) {
        return ESP_ERR_INVALID_ARG;
    }

    const motion_param_meta_t* meta = find_motion_param_meta(param_name);
    if (!meta) {
        return ESP_ERR_NOT_FOUND;
    }

    info->name = meta->name;
    info->type = CONFIG_TYPE_FLOAT;
    info->offset = meta->offset;
    info->size = sizeof(float);
    info->constraints.float_range.min = meta->min_value;
    info->constraints.float_range.max = meta->max_value;

    return ESP_OK;
}

static esp_err_t motion_ns_get_float(void* ctx, const char* param_name, float* value) {
    config_motion_limits_namespace_context_t* c = motion_ctx(ctx);
    if (!c || !c->config || !param_name || !value) {
        return ESP_ERR_INVALID_ARG;
    }

    const motion_param_meta_t* meta = find_motion_param_meta(param_name);
    if (!meta) {
        return ESP_ERR_NOT_FOUND;
    }

    const uint8_t* base = (const uint8_t*)c->config;
    const float* field_ptr = (const float*)(base + meta->offset);
    *value = *field_ptr;

    return ESP_OK;
}

static esp_err_t motion_ns_set_float(void* ctx, const char* param_name, float value) {
    config_motion_limits_namespace_context_t* c = motion_ctx(ctx);
    if (!c || !c->config || !param_name) {
        return ESP_ERR_INVALID_ARG;
    }

    const motion_param_meta_t* meta = find_motion_param_meta(param_name);
    if (!meta) {
        return ESP_ERR_NOT_FOUND;
    }

    if (value < meta->min_value || value > meta->max_value) {
        return ESP_ERR_INVALID_ARG;
    }

    if (strcmp(param_name, "min_dt") == 0 && value > c->config->max_dt) {
        return ESP_ERR_INVALID_ARG;
    }
    if (strcmp(param_name, "max_dt") == 0 && value < c->config->min_dt) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t* base = (uint8_t*)c->config;
    float* field_ptr = (float*)(base + meta->offset);
    *field_ptr = value;

    return ESP_OK;
}

const config_namespace_descriptor_t g_motion_limits_namespace_descriptor = {
    .ns_id = CONFIG_NS_MOTION_LIMITS,
    .ns_name = "motion_lim",
    .load_defaults = motion_ns_load_defaults,
    .load_from_nvs = motion_ns_load_from_nvs,
    .write_defaults_to_nvs = motion_ns_write_defaults_to_nvs,
    .save = motion_ns_save,
    .list_parameters = motion_ns_list_parameters,
    .get_parameter_info = motion_ns_get_param_info,
    .get_raw = NULL,
    .get_bool = NULL,
    .set_bool = NULL,
    .get_int32 = NULL,
    .set_int32 = NULL,
    .get_uint32 = NULL,
    .set_uint32 = NULL,
    .get_float = motion_ns_get_float,
    .set_float = motion_ns_set_float,
    .get_string = NULL,
    .set_string = NULL
};
