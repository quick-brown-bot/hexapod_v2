#include "config_ns_system.h"

#include "config_domain_defaults.h"
#include "config_domain_persistence_nvs.h"
#include "config_domain_system_access.h"
#include "config_manager_runtime_api.h"

static system_config_t g_system_config = {0};
static config_system_namespace_context_t g_system_namespace_context = {0};

void config_system_namespace_bind(
    nvs_handle_t* nvs_handle,
    bool* namespace_dirty,
    bool* namespace_loaded,
    uint16_t schema_version,
    controller_driver_type_e fallback_controller_type
) {
    g_system_namespace_context.nvs_handle = nvs_handle;
    g_system_namespace_context.namespace_dirty = namespace_dirty;
    g_system_namespace_context.namespace_loaded = namespace_loaded;
    g_system_namespace_context.config = &g_system_config;
    g_system_namespace_context.schema_version = schema_version;
    g_system_namespace_context.fallback_controller_type = fallback_controller_type;
}

void* config_system_namespace_context(void) {
    return &g_system_namespace_context;
}

system_config_t* config_system_namespace_config(void) {
    return &g_system_config;
}

const system_config_t* config_get_system(void) {
    return &g_system_config;
}

esp_err_t config_set_system(const system_config_t* config) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    g_system_config = *config;
    if (g_system_namespace_context.namespace_dirty) {
        *g_system_namespace_context.namespace_dirty = true;
    }

    esp_err_t err = config_manager_save_namespace(CONFIG_NS_SYSTEM);
    if (err == ESP_OK && g_system_namespace_context.namespace_dirty) {
        *g_system_namespace_context.namespace_dirty = false;
    }

    return err;
}

static config_system_namespace_context_t* system_ctx(void* ctx) {
    return (config_system_namespace_context_t*)ctx;
}

static esp_err_t system_ns_load_defaults(void* ctx) {
    config_system_namespace_context_t* c = system_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    config_load_system_defaults(c->config);
    return ESP_OK;
}

static esp_err_t system_ns_load_from_nvs(void* ctx) {
    config_system_namespace_context_t* c = system_ctx(ctx);
    if (!c || !c->nvs_handle || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_domain_system_load_from_nvs(
        *c->nvs_handle,
        c->config,
        c->schema_version,
        c->fallback_controller_type
    );
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

static esp_err_t system_ns_write_defaults_to_nvs(void* ctx) {
    config_system_namespace_context_t* c = system_ctx(ctx);
    if (!c || !c->nvs_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_system_write_defaults_to_nvs(*c->nvs_handle, c->schema_version);
}

static esp_err_t system_ns_save(void* ctx) {
    config_system_namespace_context_t* c = system_ctx(ctx);
    if (!c || !c->nvs_handle || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_domain_system_save_to_nvs(*c->nvs_handle, c->config);
    if (err != ESP_OK) {
        return err;
    }

    if (c->namespace_dirty) {
        *c->namespace_dirty = false;
    }

    return ESP_OK;
}

static esp_err_t system_ns_list_parameters(void* ctx, const char** param_names, size_t max_params, size_t* count) {
    (void)ctx;
    return config_domain_system_list_parameters(param_names, max_params, count);
}

static esp_err_t system_ns_get_param_info(void* ctx, const char* param_name, config_param_info_t* info) {
    (void)ctx;
    return config_domain_system_get_param_info(param_name, info);
}

static esp_err_t system_ns_get_raw(void* ctx, const char* key, void* value_out, size_t value_size) {
    config_system_namespace_context_t* c = system_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_system_get_raw(c->config, key, value_out, value_size);
}

static esp_err_t system_ns_get_bool(void* ctx, const char* param_name, bool* value) {
    config_system_namespace_context_t* c = system_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_system_get_bool(c->config, param_name, value);
}

static esp_err_t system_ns_set_bool(void* ctx, const char* param_name, bool value) {
    config_system_namespace_context_t* c = system_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_system_set_bool(c->config, param_name, value);
}

static esp_err_t system_ns_get_int32(void* ctx, const char* param_name, int32_t* value) {
    config_system_namespace_context_t* c = system_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_system_get_int32(c->config, param_name, value);
}

static esp_err_t system_ns_set_int32(void* ctx, const char* param_name, int32_t value) {
    config_system_namespace_context_t* c = system_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_system_set_int32(c->config, param_name, value);
}

static esp_err_t system_ns_get_uint32(void* ctx, const char* param_name, uint32_t* value) {
    config_system_namespace_context_t* c = system_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_system_get_uint32(c->config, param_name, value);
}

static esp_err_t system_ns_set_uint32(void* ctx, const char* param_name, uint32_t value) {
    config_system_namespace_context_t* c = system_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_system_set_uint32(c->config, param_name, value);
}

static esp_err_t system_ns_get_float(void* ctx, const char* param_name, float* value) {
    config_system_namespace_context_t* c = system_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_system_get_float(c->config, param_name, value);
}

static esp_err_t system_ns_set_float(void* ctx, const char* param_name, float value) {
    config_system_namespace_context_t* c = system_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_system_set_float(c->config, param_name, value);
}

static esp_err_t system_ns_get_string(void* ctx, const char* param_name, char* value, size_t max_len) {
    config_system_namespace_context_t* c = system_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_system_get_string(c->config, param_name, value, max_len);
}

static esp_err_t system_ns_set_string(void* ctx, const char* param_name, const char* value) {
    config_system_namespace_context_t* c = system_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_system_set_string(c->config, param_name, value);
}

const config_namespace_descriptor_t g_system_namespace_descriptor = {
    .ns_id = CONFIG_NS_SYSTEM,
    .ns_name = "system",
    .load_defaults = system_ns_load_defaults,
    .load_from_nvs = system_ns_load_from_nvs,
    .write_defaults_to_nvs = system_ns_write_defaults_to_nvs,
    .save = system_ns_save,
    .list_parameters = system_ns_list_parameters,
    .get_parameter_info = system_ns_get_param_info,
    .get_raw = system_ns_get_raw,
    .get_bool = system_ns_get_bool,
    .set_bool = system_ns_set_bool,
    .get_int32 = system_ns_get_int32,
    .set_int32 = system_ns_set_int32,
    .get_uint32 = system_ns_get_uint32,
    .set_uint32 = system_ns_set_uint32,
    .get_float = system_ns_get_float,
    .set_float = system_ns_set_float,
    .get_string = system_ns_get_string,
    .set_string = system_ns_set_string
};
