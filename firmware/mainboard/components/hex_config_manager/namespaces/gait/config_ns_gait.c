#include "config_ns_gait.h"

#include <stddef.h>
#include <string.h>

#include "config_domain_defaults.h"
#include "config_domain_persistence_nvs.h"
#include "config_manager_runtime_api.h"

typedef struct {
    const char* name;
    size_t offset;
    float min_value;
    float max_value;
} gait_param_meta_t;

static const gait_param_meta_t g_gait_param_table[] = {
    { "cycle_time_s", offsetof(gait_config_t, cycle_time_s), 0.1f, 10.0f },
    { "step_length_m", offsetof(gait_config_t, step_length_m), 0.0f, 0.5f },
    { "clearance_height_m", offsetof(gait_config_t, clearance_height_m), 0.0f, 0.3f },
    { "y_range_m", offsetof(gait_config_t, y_range_m), 0.0f, 0.3f },
    { "z_min_m", offsetof(gait_config_t, z_min_m), -0.5f, 0.5f },
    { "z_max_m", offsetof(gait_config_t, z_max_m), -0.5f, 0.5f },
    { "max_yaw_per_cycle_rad", offsetof(gait_config_t, max_yaw_per_cycle_rad), 0.0f, 3.14159f },
    { "turn_direction", offsetof(gait_config_t, turn_direction), -1.0f, 1.0f },
};

#define GAIT_PARAM_COUNT (sizeof(g_gait_param_table) / sizeof(g_gait_param_table[0]))

static gait_config_t g_gait_namespace_config = {0};
static config_gait_namespace_context_t g_gait_namespace_context = {0};

void config_gait_namespace_bind(
    nvs_handle_t* nvs_handle,
    bool* namespace_dirty,
    bool* namespace_loaded
) {
    g_gait_namespace_context.nvs_handle = nvs_handle;
    g_gait_namespace_context.namespace_dirty = namespace_dirty;
    g_gait_namespace_context.namespace_loaded = namespace_loaded;
    g_gait_namespace_context.config = &g_gait_namespace_config;
}

void* config_gait_namespace_context(void) {
    return &g_gait_namespace_context;
}

gait_config_t* config_gait_namespace_config(void) {
    return &g_gait_namespace_config;
}

const gait_config_t* config_get_gait(void) {
    return &g_gait_namespace_config;
}

esp_err_t config_set_gait(const gait_config_t* config) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    g_gait_namespace_config = *config;
    if (g_gait_namespace_context.namespace_dirty) {
        *g_gait_namespace_context.namespace_dirty = true;
    }

    esp_err_t err = config_manager_save_namespace(CONFIG_NS_GAIT);
    if (err == ESP_OK && g_gait_namespace_context.namespace_dirty) {
        *g_gait_namespace_context.namespace_dirty = false;
    }

    return err;
}

static config_gait_namespace_context_t* gait_ctx(void* ctx) {
    return (config_gait_namespace_context_t*)ctx;
}

static const gait_param_meta_t* find_gait_param(const char* param_name) {
    if (!param_name) {
        return NULL;
    }

    for (size_t i = 0; i < GAIT_PARAM_COUNT; i++) {
        if (strcmp(param_name, g_gait_param_table[i].name) == 0) {
            return &g_gait_param_table[i];
        }
    }

    return NULL;
}

static esp_err_t gait_ns_load_defaults(void* ctx) {
    config_gait_namespace_context_t* c = gait_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    config_load_gait_defaults(c->config);
    return ESP_OK;
}

static esp_err_t gait_ns_load_from_nvs(void* ctx) {
    config_gait_namespace_context_t* c = gait_ctx(ctx);
    if (!c || !c->nvs_handle || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_domain_gait_load_from_nvs(*c->nvs_handle, c->config);
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

static esp_err_t gait_ns_write_defaults_to_nvs(void* ctx) {
    config_gait_namespace_context_t* c = gait_ctx(ctx);
    if (!c || !c->nvs_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_gait_write_defaults_to_nvs(*c->nvs_handle);
}

static esp_err_t gait_ns_save(void* ctx) {
    config_gait_namespace_context_t* c = gait_ctx(ctx);
    if (!c || !c->nvs_handle || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_domain_gait_save_to_nvs(*c->nvs_handle, c->config);
    if (err != ESP_OK) {
        return err;
    }

    if (c->namespace_dirty) {
        *c->namespace_dirty = false;
    }

    return ESP_OK;
}

static esp_err_t gait_ns_list_parameters(void* ctx, const char** param_names, size_t max_params, size_t* count) {
    (void)ctx;

    if (!param_names || !count) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t to_emit = GAIT_PARAM_COUNT;
    if (to_emit > max_params) {
        to_emit = max_params;
    }

    for (size_t i = 0; i < to_emit; i++) {
        param_names[i] = g_gait_param_table[i].name;
    }

    *count = to_emit;
    return ESP_OK;
}

static esp_err_t gait_ns_get_param_info(void* ctx, const char* param_name, config_param_info_t* info) {
    (void)ctx;

    if (!param_name || !info) {
        return ESP_ERR_INVALID_ARG;
    }

    const gait_param_meta_t* meta = find_gait_param(param_name);
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

static esp_err_t gait_ns_get_float(void* ctx, const char* param_name, float* value) {
    config_gait_namespace_context_t* c = gait_ctx(ctx);
    if (!c || !c->config || !param_name || !value) {
        return ESP_ERR_INVALID_ARG;
    }

    const gait_param_meta_t* meta = find_gait_param(param_name);
    if (!meta) {
        return ESP_ERR_NOT_FOUND;
    }

    const uint8_t* base = (const uint8_t*)c->config;
    const float* field_ptr = (const float*)(base + meta->offset);
    *value = *field_ptr;

    return ESP_OK;
}

static esp_err_t gait_ns_set_float(void* ctx, const char* param_name, float value) {
    config_gait_namespace_context_t* c = gait_ctx(ctx);
    if (!c || !c->config || !param_name) {
        return ESP_ERR_INVALID_ARG;
    }

    const gait_param_meta_t* meta = find_gait_param(param_name);
    if (!meta) {
        return ESP_ERR_NOT_FOUND;
    }

    if (value < meta->min_value || value > meta->max_value) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t* base = (uint8_t*)c->config;
    float* field_ptr = (float*)(base + meta->offset);
    *field_ptr = value;

    return ESP_OK;
}

const config_namespace_descriptor_t g_gait_namespace_descriptor = {
    .ns_id = CONFIG_NS_GAIT,
    .ns_name = "gait",
    .load_defaults = gait_ns_load_defaults,
    .load_from_nvs = gait_ns_load_from_nvs,
    .write_defaults_to_nvs = gait_ns_write_defaults_to_nvs,
    .save = gait_ns_save,
    .list_parameters = gait_ns_list_parameters,
    .get_parameter_info = gait_ns_get_param_info,
    .get_raw = NULL,
    .get_bool = NULL,
    .set_bool = NULL,
    .get_int32 = NULL,
    .set_int32 = NULL,
    .get_uint32 = NULL,
    .set_uint32 = NULL,
    .get_float = gait_ns_get_float,
    .set_float = gait_ns_set_float,
    .get_string = NULL,
    .set_string = NULL
};
