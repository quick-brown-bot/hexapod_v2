#include "config_ns_controller.h"

#include "config_domain_controller_access.h"
#include "config_domain_defaults.h"
#include "config_manager_runtime_api.h"
#include "config_domain_persistence_nvs.h"

static controller_config_namespace_t g_controller_namespace_config = {0};
static config_controller_namespace_context_t g_controller_namespace_context = {0};

void config_controller_namespace_bind(
    nvs_handle_t* nvs_handle,
    bool* namespace_dirty,
    bool* namespace_loaded
) {
    g_controller_namespace_context.nvs_handle = nvs_handle;
    g_controller_namespace_context.namespace_dirty = namespace_dirty;
    g_controller_namespace_context.namespace_loaded = namespace_loaded;
    g_controller_namespace_context.config = &g_controller_namespace_config;
}

void* config_controller_namespace_context(void) {
    return &g_controller_namespace_context;
}

controller_config_namespace_t* config_controller_namespace_config(void) {
    return &g_controller_namespace_config;
}

const controller_config_namespace_t* config_get_controller(void) {
    return &g_controller_namespace_config;
}

esp_err_t config_set_controller(const controller_config_namespace_t* config) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    g_controller_namespace_config = *config;
    if (g_controller_namespace_context.namespace_dirty) {
        *g_controller_namespace_context.namespace_dirty = true;
    }

    esp_err_t err = config_manager_save_namespace(CONFIG_NS_CONTROLLER);
    if (err == ESP_OK && g_controller_namespace_context.namespace_dirty) {
        *g_controller_namespace_context.namespace_dirty = false;
    }

    return err;
}

static config_controller_namespace_context_t* controller_ctx(void* ctx) {
    return (config_controller_namespace_context_t*)ctx;
}

static esp_err_t controller_ns_load_defaults(void* ctx) {
    config_controller_namespace_context_t* c = controller_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    config_load_controller_defaults(c->config);
    return ESP_OK;
}

static esp_err_t controller_ns_load_from_nvs(void* ctx) {
    config_controller_namespace_context_t* c = controller_ctx(ctx);
    if (!c || !c->nvs_handle || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_domain_controller_load_from_nvs(*c->nvs_handle, c->config);
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

static esp_err_t controller_ns_write_defaults_to_nvs(void* ctx) {
    config_controller_namespace_context_t* c = controller_ctx(ctx);
    if (!c || !c->nvs_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_controller_write_defaults_to_nvs(*c->nvs_handle);
}

static esp_err_t controller_ns_save(void* ctx) {
    config_controller_namespace_context_t* c = controller_ctx(ctx);
    if (!c || !c->nvs_handle || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_domain_controller_save_to_nvs(*c->nvs_handle, c->config);
    if (err != ESP_OK) {
        return err;
    }

    if (c->namespace_dirty) {
        *c->namespace_dirty = false;
    }

    return ESP_OK;
}

static esp_err_t controller_ns_list_parameters(void* ctx, const char** param_names, size_t max_params, size_t* count) {
    (void)ctx;
    return config_domain_controller_list_parameters(param_names, max_params, count);
}

static esp_err_t controller_ns_get_param_info(void* ctx, const char* param_name, config_param_info_t* info) {
    (void)ctx;
    return config_domain_controller_get_param_info(param_name, info);
}

static esp_err_t controller_ns_get_int32(void* ctx, const char* param_name, int32_t* value) {
    config_controller_namespace_context_t* c = controller_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_controller_get_int32(c->config, param_name, value);
}

static esp_err_t controller_ns_get_bool(void* ctx, const char* param_name, bool* value) {
    config_controller_namespace_context_t* c = controller_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_controller_get_bool(c->config, param_name, value);
}

static esp_err_t controller_ns_set_bool(void* ctx, const char* param_name, bool value) {
    config_controller_namespace_context_t* c = controller_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_controller_set_bool(c->config, param_name, value);
}

static esp_err_t controller_ns_set_int32(void* ctx, const char* param_name, int32_t value) {
    config_controller_namespace_context_t* c = controller_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_controller_set_int32(c->config, param_name, value);
}

static esp_err_t controller_ns_get_uint32(void* ctx, const char* param_name, uint32_t* value) {
    config_controller_namespace_context_t* c = controller_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_controller_get_uint32(c->config, param_name, value);
}

static esp_err_t controller_ns_set_uint32(void* ctx, const char* param_name, uint32_t value) {
    config_controller_namespace_context_t* c = controller_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_controller_set_uint32(c->config, param_name, value);
}

static esp_err_t controller_ns_get_float(void* ctx, const char* param_name, float* value) {
    config_controller_namespace_context_t* c = controller_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_controller_get_float(c->config, param_name, value);
}

static esp_err_t controller_ns_set_float(void* ctx, const char* param_name, float value) {
    config_controller_namespace_context_t* c = controller_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_controller_set_float(c->config, param_name, value);
}

const config_namespace_descriptor_t g_controller_namespace_descriptor = {
    .ns_id = CONFIG_NS_CONTROLLER,
    .ns_name = "controller",
    .load_defaults = controller_ns_load_defaults,
    .load_from_nvs = controller_ns_load_from_nvs,
    .write_defaults_to_nvs = controller_ns_write_defaults_to_nvs,
    .save = controller_ns_save,
    .list_parameters = controller_ns_list_parameters,
    .get_parameter_info = controller_ns_get_param_info,
    .get_raw = NULL,
    .get_bool = controller_ns_get_bool,
    .set_bool = controller_ns_set_bool,
    .get_int32 = controller_ns_get_int32,
    .set_int32 = controller_ns_set_int32,
    .get_uint32 = controller_ns_get_uint32,
    .set_uint32 = controller_ns_set_uint32,
    .get_float = controller_ns_get_float,
    .set_float = controller_ns_set_float,
    .get_string = NULL,
    .set_string = NULL
};
