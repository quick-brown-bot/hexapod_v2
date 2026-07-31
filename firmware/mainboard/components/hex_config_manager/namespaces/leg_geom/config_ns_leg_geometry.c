#include "config_ns_leg_geometry.h"

#include <stdio.h>
#include <string.h>

#include "config_domain_defaults.h"
#include "config_manager_runtime_api.h"
#include "config_domain_persistence_nvs.h"

#define LEG_GEOM_PARAM_COUNT 9
#define LEG_GEOM_PARAM_NAME_CAPACITY (NUM_LEGS * LEG_GEOM_PARAM_COUNT)

typedef enum {
    LEG_GEOM_PARAM_LEN_COXA = 0,
    LEG_GEOM_PARAM_LEN_FEMUR,
    LEG_GEOM_PARAM_LEN_TIBIA,
    LEG_GEOM_PARAM_MOUNT_X,
    LEG_GEOM_PARAM_MOUNT_Y,
    LEG_GEOM_PARAM_MOUNT_Z,
    LEG_GEOM_PARAM_MOUNT_YAW,
    LEG_GEOM_PARAM_STANCE_OUT,
    LEG_GEOM_PARAM_STANCE_FWD,
    LEG_GEOM_PARAM_INVALID
} leg_geom_param_type_t;

static const char* LEG_GEOM_PARAM_SUFFIXES[LEG_GEOM_PARAM_COUNT] = {
    "len_coxa",
    "len_femur",
    "len_tibia",
    "mount_x",
    "mount_y",
    "mount_z",
    "mount_yaw",
    "stance_out",
    "stance_fwd"
};

static char g_leg_geom_param_names[LEG_GEOM_PARAM_NAME_CAPACITY][32];
static leg_geometry_config_t g_leg_geometry_namespace_config = {0};
static config_leg_geometry_namespace_context_t g_leg_geometry_namespace_context = {0};

void config_leg_geometry_namespace_bind(
    nvs_handle_t* nvs_handle,
    bool* namespace_dirty,
    bool* namespace_loaded
) {
    g_leg_geometry_namespace_context.nvs_handle = nvs_handle;
    g_leg_geometry_namespace_context.namespace_dirty = namespace_dirty;
    g_leg_geometry_namespace_context.namespace_loaded = namespace_loaded;
    g_leg_geometry_namespace_context.config = &g_leg_geometry_namespace_config;
}

void* config_leg_geometry_namespace_context(void) {
    return &g_leg_geometry_namespace_context;
}

leg_geometry_config_t* config_leg_geometry_namespace_config(void) {
    return &g_leg_geometry_namespace_config;
}

const leg_geometry_config_t* config_get_leg_geometry(void) {
    return &g_leg_geometry_namespace_config;
}

esp_err_t config_set_leg_geometry(const leg_geometry_config_t* config) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    g_leg_geometry_namespace_config = *config;
    if (g_leg_geometry_namespace_context.namespace_dirty) {
        *g_leg_geometry_namespace_context.namespace_dirty = true;
    }

    esp_err_t err = config_manager_save_namespace(CONFIG_NS_LEG_GEOMETRY);
    if (err == ESP_OK && g_leg_geometry_namespace_context.namespace_dirty) {
        *g_leg_geometry_namespace_context.namespace_dirty = false;
    }

    return err;
}

static config_leg_geometry_namespace_context_t* leg_geom_ctx(void* ctx) {
    return (config_leg_geometry_namespace_context_t*)ctx;
}

static esp_err_t parse_leg_geom_param_name(
    const char* param_name,
    int* leg_index,
    leg_geom_param_type_t* param_type,
    const char** param_suffix
) {
    if (!param_name || !leg_index || !param_type) {
        return ESP_ERR_INVALID_ARG;
    }

    int leg = 0;
    char suffix[24] = {0};
    int parsed = sscanf(param_name, "leg%d_%23s", &leg, suffix);
    if (parsed != 2) {
        return ESP_ERR_INVALID_ARG;
    }

    if (leg < 0 || leg >= NUM_LEGS) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < LEG_GEOM_PARAM_COUNT; i++) {
        if (strcmp(suffix, LEG_GEOM_PARAM_SUFFIXES[i]) == 0) {
            *leg_index = leg;
            *param_type = (leg_geom_param_type_t)i;
            if (param_suffix) {
                *param_suffix = LEG_GEOM_PARAM_SUFFIXES[i];
            }
            return ESP_OK;
        }
    }

    return ESP_ERR_INVALID_ARG;
}

static float* get_field_ptr(leg_geometry_config_t* config, int leg, leg_geom_param_type_t param_type) {
    switch (param_type) {
        case LEG_GEOM_PARAM_LEN_COXA: return &config->len_coxa[leg];
        case LEG_GEOM_PARAM_LEN_FEMUR: return &config->len_femur[leg];
        case LEG_GEOM_PARAM_LEN_TIBIA: return &config->len_tibia[leg];
        case LEG_GEOM_PARAM_MOUNT_X: return &config->mount_x[leg];
        case LEG_GEOM_PARAM_MOUNT_Y: return &config->mount_y[leg];
        case LEG_GEOM_PARAM_MOUNT_Z: return &config->mount_z[leg];
        case LEG_GEOM_PARAM_MOUNT_YAW: return &config->mount_yaw[leg];
        case LEG_GEOM_PARAM_STANCE_OUT: return &config->stance_out[leg];
        case LEG_GEOM_PARAM_STANCE_FWD: return &config->stance_fwd[leg];
        default: return NULL;
    }
}

static const float* get_field_const_ptr(const leg_geometry_config_t* config, int leg, leg_geom_param_type_t param_type) {
    return get_field_ptr((leg_geometry_config_t*)config, leg, param_type);
}

static void fill_param_constraints(leg_geom_param_type_t param_type, config_param_info_t* info) {
    switch (param_type) {
        case LEG_GEOM_PARAM_LEN_COXA:
        case LEG_GEOM_PARAM_LEN_FEMUR:
        case LEG_GEOM_PARAM_LEN_TIBIA:
            info->constraints.float_range.min = 0.01f;
            info->constraints.float_range.max = 1.00f;
            break;
        case LEG_GEOM_PARAM_MOUNT_YAW:
            info->constraints.float_range.min = -6.2831853f;
            info->constraints.float_range.max = 6.2831853f;
            break;
        default:
            info->constraints.float_range.min = -1.0f;
            info->constraints.float_range.max = 1.0f;
            break;
    }
}

static esp_err_t leg_geom_ns_load_defaults(void* ctx) {
    config_leg_geometry_namespace_context_t* c = leg_geom_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    config_load_leg_geometry_defaults(c->config);
    return ESP_OK;
}

static esp_err_t leg_geom_ns_load_from_nvs(void* ctx) {
    config_leg_geometry_namespace_context_t* c = leg_geom_ctx(ctx);
    if (!c || !c->nvs_handle || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_domain_leg_geometry_load_from_nvs(*c->nvs_handle, c->config);
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

static esp_err_t leg_geom_ns_write_defaults_to_nvs(void* ctx) {
    config_leg_geometry_namespace_context_t* c = leg_geom_ctx(ctx);
    if (!c || !c->nvs_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_leg_geometry_write_defaults_to_nvs(*c->nvs_handle);
}

static esp_err_t leg_geom_ns_save(void* ctx) {
    config_leg_geometry_namespace_context_t* c = leg_geom_ctx(ctx);
    if (!c || !c->nvs_handle || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_domain_leg_geometry_save_to_nvs(*c->nvs_handle, c->config);
    if (err != ESP_OK) {
        return err;
    }

    if (c->namespace_dirty) {
        *c->namespace_dirty = false;
    }

    return ESP_OK;
}

static esp_err_t leg_geom_ns_list_parameters(void* ctx, const char** param_names, size_t max_params, size_t* count) {
    (void)ctx;

    if (!param_names || !count) {
        return ESP_ERR_INVALID_ARG;
    }

    const size_t total = LEG_GEOM_PARAM_NAME_CAPACITY;
    size_t to_emit = total;
    if (to_emit > max_params) {
        to_emit = max_params;
    }

    size_t idx = 0;
    for (int leg = 0; leg < NUM_LEGS && idx < to_emit; leg++) {
        for (int p = 0; p < LEG_GEOM_PARAM_COUNT && idx < to_emit; p++) {
            snprintf(
                g_leg_geom_param_names[idx],
                sizeof(g_leg_geom_param_names[0]),
                "leg%d_%s",
                leg,
                LEG_GEOM_PARAM_SUFFIXES[p]
            );
            param_names[idx] = g_leg_geom_param_names[idx];
            idx++;
        }
    }

    *count = idx;
    return ESP_OK;
}

static esp_err_t leg_geom_ns_get_param_info(void* ctx, const char* param_name, config_param_info_t* info) {
    (void)ctx;

    if (!param_name || !info) {
        return ESP_ERR_INVALID_ARG;
    }

    int leg = 0;
    leg_geom_param_type_t param_type = LEG_GEOM_PARAM_INVALID;
    const char* canonical_suffix = NULL;
    esp_err_t err = parse_leg_geom_param_name(param_name, &leg, &param_type, &canonical_suffix);
    if (err != ESP_OK || !canonical_suffix) {
        return ESP_ERR_NOT_FOUND;
    }

    info->name = param_name;
    info->type = CONFIG_TYPE_FLOAT;
    info->size = sizeof(float);

    switch (param_type) {
        case LEG_GEOM_PARAM_LEN_COXA:
            info->offset = offsetof(leg_geometry_config_t, len_coxa) + ((size_t)leg * sizeof(float));
            break;
        case LEG_GEOM_PARAM_LEN_FEMUR:
            info->offset = offsetof(leg_geometry_config_t, len_femur) + ((size_t)leg * sizeof(float));
            break;
        case LEG_GEOM_PARAM_LEN_TIBIA:
            info->offset = offsetof(leg_geometry_config_t, len_tibia) + ((size_t)leg * sizeof(float));
            break;
        case LEG_GEOM_PARAM_MOUNT_X:
            info->offset = offsetof(leg_geometry_config_t, mount_x) + ((size_t)leg * sizeof(float));
            break;
        case LEG_GEOM_PARAM_MOUNT_Y:
            info->offset = offsetof(leg_geometry_config_t, mount_y) + ((size_t)leg * sizeof(float));
            break;
        case LEG_GEOM_PARAM_MOUNT_Z:
            info->offset = offsetof(leg_geometry_config_t, mount_z) + ((size_t)leg * sizeof(float));
            break;
        case LEG_GEOM_PARAM_MOUNT_YAW:
            info->offset = offsetof(leg_geometry_config_t, mount_yaw) + ((size_t)leg * sizeof(float));
            break;
        case LEG_GEOM_PARAM_STANCE_OUT:
            info->offset = offsetof(leg_geometry_config_t, stance_out) + ((size_t)leg * sizeof(float));
            break;
        case LEG_GEOM_PARAM_STANCE_FWD:
            info->offset = offsetof(leg_geometry_config_t, stance_fwd) + ((size_t)leg * sizeof(float));
            break;
        default:
            return ESP_ERR_NOT_FOUND;
    }

    fill_param_constraints(param_type, info);
    return ESP_OK;
}

static esp_err_t leg_geom_ns_get_float(void* ctx, const char* param_name, float* value) {
    config_leg_geometry_namespace_context_t* c = leg_geom_ctx(ctx);
    if (!c || !c->config || !param_name || !value) {
        return ESP_ERR_INVALID_ARG;
    }

    int leg = 0;
    leg_geom_param_type_t param_type = LEG_GEOM_PARAM_INVALID;
    esp_err_t err = parse_leg_geom_param_name(param_name, &leg, &param_type, NULL);
    if (err != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }

    const float* field_ptr = get_field_const_ptr(c->config, leg, param_type);
    if (!field_ptr) {
        return ESP_ERR_NOT_FOUND;
    }

    *value = *field_ptr;
    return ESP_OK;
}

static esp_err_t leg_geom_ns_set_float(void* ctx, const char* param_name, float value) {
    config_leg_geometry_namespace_context_t* c = leg_geom_ctx(ctx);
    if (!c || !c->config || !param_name) {
        return ESP_ERR_INVALID_ARG;
    }

    int leg = 0;
    leg_geom_param_type_t param_type = LEG_GEOM_PARAM_INVALID;
    esp_err_t err = parse_leg_geom_param_name(param_name, &leg, &param_type, NULL);
    if (err != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }

    config_param_info_t info = {0};
    err = leg_geom_ns_get_param_info(ctx, param_name, &info);
    if (err != ESP_OK) {
        return err;
    }

    if (value < info.constraints.float_range.min || value > info.constraints.float_range.max) {
        return ESP_ERR_INVALID_ARG;
    }

    float* field_ptr = get_field_ptr(c->config, leg, param_type);
    if (!field_ptr) {
        return ESP_ERR_NOT_FOUND;
    }

    *field_ptr = value;
    return ESP_OK;
}

const config_namespace_descriptor_t g_leg_geometry_namespace_descriptor = {
    .ns_id = CONFIG_NS_LEG_GEOMETRY,
    .ns_name = "leg_geom",
    .load_defaults = leg_geom_ns_load_defaults,
    .load_from_nvs = leg_geom_ns_load_from_nvs,
    .write_defaults_to_nvs = leg_geom_ns_write_defaults_to_nvs,
    .save = leg_geom_ns_save,
    .list_parameters = leg_geom_ns_list_parameters,
    .get_parameter_info = leg_geom_ns_get_param_info,
    .get_raw = NULL,
    .get_bool = NULL,
    .set_bool = NULL,
    .get_int32 = NULL,
    .set_int32 = NULL,
    .get_uint32 = NULL,
    .set_uint32 = NULL,
    .get_float = leg_geom_ns_get_float,
    .set_float = leg_geom_ns_set_float,
    .get_string = NULL,
    .set_string = NULL
};
