#include "config_ns_servo_map.h"

#include <stdio.h>
#include <string.h>

#include "config_domain_defaults.h"
#include "config_domain_persistence_nvs.h"
#include "config_manager_runtime_api.h"

#define JOINT_NAME_COUNT NUM_JOINTS_PER_LEG
#define SMAP_PARAM_COUNT_PER_LEG (1 + (JOINT_NAME_COUNT * 2))
#define SMAP_PARAM_COUNT (NUM_LEGS * SMAP_PARAM_COUNT_PER_LEG)

typedef enum {
    SMAP_PARAM_GROUP = 0,
    SMAP_PARAM_GPIO,
    SMAP_PARAM_DRIVER
} servo_map_param_type_t;

typedef struct {
    char name[32];
    int leg;
    int joint;
    servo_map_param_type_t type;
} servo_map_param_entry_t;

static const char* JOINT_NAMES[JOINT_NAME_COUNT] = { "coxa", "femur", "tibia" };

static servo_map_param_entry_t g_servo_map_param_table[SMAP_PARAM_COUNT];
static bool g_servo_map_param_table_initialized = false;

static servo_map_config_t g_servo_map_namespace_config = {0};
static config_servo_map_namespace_context_t g_servo_map_namespace_context = {0};

void config_servo_map_namespace_bind(
    nvs_handle_t* nvs_handle,
    bool* namespace_dirty,
    bool* namespace_loaded
) {
    g_servo_map_namespace_context.nvs_handle = nvs_handle;
    g_servo_map_namespace_context.namespace_dirty = namespace_dirty;
    g_servo_map_namespace_context.namespace_loaded = namespace_loaded;
    g_servo_map_namespace_context.config = &g_servo_map_namespace_config;
}

void* config_servo_map_namespace_context(void) {
    return &g_servo_map_namespace_context;
}

servo_map_config_t* config_servo_map_namespace_config(void) {
    return &g_servo_map_namespace_config;
}

const servo_map_config_t* config_get_servo_map(void) {
    return &g_servo_map_namespace_config;
}

esp_err_t config_set_servo_map(const servo_map_config_t* config) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    g_servo_map_namespace_config = *config;
    if (g_servo_map_namespace_context.namespace_dirty) {
        *g_servo_map_namespace_context.namespace_dirty = true;
    }

    esp_err_t err = config_manager_save_namespace(CONFIG_NS_SERVO_MAP);
    if (err == ESP_OK && g_servo_map_namespace_context.namespace_dirty) {
        *g_servo_map_namespace_context.namespace_dirty = false;
    }

    return err;
}

static config_servo_map_namespace_context_t* servo_map_ctx(void* ctx) {
    return (config_servo_map_namespace_context_t*)ctx;
}

static void ensure_param_table_initialized(void) {
    if (g_servo_map_param_table_initialized) {
        return;
    }

    size_t idx = 0;
    for (int leg = 0; leg < NUM_LEGS; leg++) {
        snprintf(g_servo_map_param_table[idx].name, sizeof(g_servo_map_param_table[idx].name), "leg%d_group", leg);
        g_servo_map_param_table[idx].leg = leg;
        g_servo_map_param_table[idx].joint = -1;
        g_servo_map_param_table[idx].type = SMAP_PARAM_GROUP;
        idx++;

        for (int joint = 0; joint < JOINT_NAME_COUNT; joint++) {
            snprintf(g_servo_map_param_table[idx].name, sizeof(g_servo_map_param_table[idx].name), "leg%d_%s_gpio", leg, JOINT_NAMES[joint]);
            g_servo_map_param_table[idx].leg = leg;
            g_servo_map_param_table[idx].joint = joint;
            g_servo_map_param_table[idx].type = SMAP_PARAM_GPIO;
            idx++;

            snprintf(g_servo_map_param_table[idx].name, sizeof(g_servo_map_param_table[idx].name), "leg%d_%s_driver", leg, JOINT_NAMES[joint]);
            g_servo_map_param_table[idx].leg = leg;
            g_servo_map_param_table[idx].joint = joint;
            g_servo_map_param_table[idx].type = SMAP_PARAM_DRIVER;
            idx++;
        }
    }

    g_servo_map_param_table_initialized = true;
}

static const servo_map_param_entry_t* find_param_entry(const char* param_name) {
    if (!param_name) {
        return NULL;
    }

    ensure_param_table_initialized();

    for (size_t i = 0; i < SMAP_PARAM_COUNT; i++) {
        if (strcmp(g_servo_map_param_table[i].name, param_name) == 0) {
            return &g_servo_map_param_table[i];
        }
    }

    return NULL;
}

static int32_t* field_ptr(servo_map_config_t* config, const servo_map_param_entry_t* entry) {
    if (!config || !entry) {
        return NULL;
    }

    switch (entry->type) {
        case SMAP_PARAM_GROUP:
            return &config->mcpwm_group_id[entry->leg];
        case SMAP_PARAM_GPIO:
            return &config->servo_gpio[entry->leg][entry->joint];
        case SMAP_PARAM_DRIVER:
            return &config->servo_driver_sel[entry->leg][entry->joint];
        default:
            return NULL;
    }
}

static void param_constraints(const servo_map_param_entry_t* entry, int32_t* min_out, int32_t* max_out) {
    if (!entry || !min_out || !max_out) {
        return;
    }

    switch (entry->type) {
        case SMAP_PARAM_GROUP:
            *min_out = 0;
            *max_out = 1;
            break;
        case SMAP_PARAM_DRIVER:
            *min_out = 0;
            *max_out = 1;
            break;
        case SMAP_PARAM_GPIO:
        default:
            *min_out = -1;
            *max_out = 39;
            break;
    }
}

static esp_err_t servo_map_ns_load_defaults(void* ctx) {
    config_servo_map_namespace_context_t* c = servo_map_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    config_load_servo_map_defaults(c->config);
    return ESP_OK;
}

static esp_err_t servo_map_ns_load_from_nvs(void* ctx) {
    config_servo_map_namespace_context_t* c = servo_map_ctx(ctx);
    if (!c || !c->nvs_handle || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_domain_servo_map_load_from_nvs(*c->nvs_handle, c->config);
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

static esp_err_t servo_map_ns_write_defaults_to_nvs(void* ctx) {
    config_servo_map_namespace_context_t* c = servo_map_ctx(ctx);
    if (!c || !c->nvs_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_servo_map_write_defaults_to_nvs(*c->nvs_handle);
}

static esp_err_t servo_map_ns_save(void* ctx) {
    config_servo_map_namespace_context_t* c = servo_map_ctx(ctx);
    if (!c || !c->nvs_handle || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_domain_servo_map_save_to_nvs(*c->nvs_handle, c->config);
    if (err != ESP_OK) {
        return err;
    }

    if (c->namespace_dirty) {
        *c->namespace_dirty = false;
    }

    return ESP_OK;
}

static esp_err_t servo_map_ns_list_parameters(void* ctx, const char** param_names, size_t max_params, size_t* count) {
    (void)ctx;

    if (!param_names || !count) {
        return ESP_ERR_INVALID_ARG;
    }

    ensure_param_table_initialized();

    size_t to_emit = SMAP_PARAM_COUNT;
    if (to_emit > max_params) {
        to_emit = max_params;
    }

    for (size_t i = 0; i < to_emit; i++) {
        param_names[i] = g_servo_map_param_table[i].name;
    }

    *count = to_emit;
    return ESP_OK;
}

static esp_err_t servo_map_ns_get_param_info(void* ctx, const char* param_name, config_param_info_t* info) {
    (void)ctx;

    if (!param_name || !info) {
        return ESP_ERR_INVALID_ARG;
    }

    const servo_map_param_entry_t* entry = find_param_entry(param_name);
    if (!entry) {
        return ESP_ERR_NOT_FOUND;
    }

    int32_t min_value = 0;
    int32_t max_value = 0;
    param_constraints(entry, &min_value, &max_value);

    info->name = entry->name;
    info->type = CONFIG_TYPE_INT32;
    info->offset = 0;
    info->size = sizeof(int32_t);
    info->constraints.int_range.min = min_value;
    info->constraints.int_range.max = max_value;

    return ESP_OK;
}

static esp_err_t servo_map_ns_get_int32(void* ctx, const char* param_name, int32_t* value) {
    config_servo_map_namespace_context_t* c = servo_map_ctx(ctx);
    if (!c || !c->config || !param_name || !value) {
        return ESP_ERR_INVALID_ARG;
    }

    const servo_map_param_entry_t* entry = find_param_entry(param_name);
    if (!entry) {
        return ESP_ERR_NOT_FOUND;
    }

    int32_t* ptr = field_ptr(c->config, entry);
    if (!ptr) {
        return ESP_ERR_NOT_FOUND;
    }

    *value = *ptr;
    return ESP_OK;
}

static esp_err_t servo_map_ns_set_int32(void* ctx, const char* param_name, int32_t value) {
    config_servo_map_namespace_context_t* c = servo_map_ctx(ctx);
    if (!c || !c->config || !param_name) {
        return ESP_ERR_INVALID_ARG;
    }

    const servo_map_param_entry_t* entry = find_param_entry(param_name);
    if (!entry) {
        return ESP_ERR_NOT_FOUND;
    }

    int32_t min_value = 0;
    int32_t max_value = 0;
    param_constraints(entry, &min_value, &max_value);
    if (value < min_value || value > max_value) {
        return ESP_ERR_INVALID_ARG;
    }

    int32_t* ptr = field_ptr(c->config, entry);
    if (!ptr) {
        return ESP_ERR_NOT_FOUND;
    }

    *ptr = value;
    return ESP_OK;
}

const config_namespace_descriptor_t g_servo_map_namespace_descriptor = {
    .ns_id = CONFIG_NS_SERVO_MAP,
    .ns_name = "servo_map",
    .load_defaults = servo_map_ns_load_defaults,
    .load_from_nvs = servo_map_ns_load_from_nvs,
    .write_defaults_to_nvs = servo_map_ns_write_defaults_to_nvs,
    .save = servo_map_ns_save,
    .list_parameters = servo_map_ns_list_parameters,
    .get_parameter_info = servo_map_ns_get_param_info,
    .get_raw = NULL,
    .get_bool = NULL,
    .set_bool = NULL,
    .get_int32 = servo_map_ns_get_int32,
    .set_int32 = servo_map_ns_set_int32,
    .get_uint32 = NULL,
    .set_uint32 = NULL,
    .get_float = NULL,
    .set_float = NULL,
    .get_string = NULL,
    .set_string = NULL
};
