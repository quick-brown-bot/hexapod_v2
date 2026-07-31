#include "config_ns_wifi.h"

#include "config_domain_defaults.h"
#include "config_manager_runtime_api.h"
#include "config_domain_persistence_nvs.h"
#include "config_domain_wifi_access.h"

static wifi_config_namespace_t g_wifi_namespace_config = {0};
static config_wifi_namespace_context_t g_wifi_namespace_context = {0};

void config_wifi_namespace_bind(
    nvs_handle_t* nvs_handle,
    bool* namespace_dirty,
    bool* namespace_loaded
) {
    g_wifi_namespace_context.nvs_handle = nvs_handle;
    g_wifi_namespace_context.namespace_dirty = namespace_dirty;
    g_wifi_namespace_context.namespace_loaded = namespace_loaded;
    g_wifi_namespace_context.config = &g_wifi_namespace_config;
}

void* config_wifi_namespace_context(void) {
    return &g_wifi_namespace_context;
}

wifi_config_namespace_t* config_wifi_namespace_config(void) {
    return &g_wifi_namespace_config;
}

const wifi_config_namespace_t* config_get_wifi(void) {
    return &g_wifi_namespace_config;
}

esp_err_t config_set_wifi(const wifi_config_namespace_t* config) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    g_wifi_namespace_config = *config;
    if (g_wifi_namespace_context.namespace_dirty) {
        *g_wifi_namespace_context.namespace_dirty = true;
    }

    esp_err_t err = config_manager_save_namespace(CONFIG_NS_WIFI);
    if (err == ESP_OK && g_wifi_namespace_context.namespace_dirty) {
        *g_wifi_namespace_context.namespace_dirty = false;
    }

    return err;
}

static config_wifi_namespace_context_t* wifi_ctx(void* ctx) {
    return (config_wifi_namespace_context_t*)ctx;
}

static esp_err_t wifi_ns_load_defaults(void* ctx) {
    config_wifi_namespace_context_t* c = wifi_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    config_load_wifi_defaults(c->config);
    return ESP_OK;
}

static esp_err_t wifi_ns_load_from_nvs(void* ctx) {
    config_wifi_namespace_context_t* c = wifi_ctx(ctx);
    if (!c || !c->nvs_handle || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_domain_wifi_load_from_nvs(*c->nvs_handle, c->config);
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

static esp_err_t wifi_ns_write_defaults_to_nvs(void* ctx) {
    config_wifi_namespace_context_t* c = wifi_ctx(ctx);
    if (!c || !c->nvs_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_wifi_write_defaults_to_nvs(*c->nvs_handle);
}

static esp_err_t wifi_ns_save(void* ctx) {
    config_wifi_namespace_context_t* c = wifi_ctx(ctx);
    if (!c || !c->nvs_handle || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_domain_wifi_save_to_nvs(*c->nvs_handle, c->config);
    if (err != ESP_OK) {
        return err;
    }

    if (c->namespace_dirty) {
        *c->namespace_dirty = false;
    }

    return ESP_OK;
}

static esp_err_t wifi_ns_list_parameters(void* ctx, const char** param_names, size_t max_params, size_t* count) {
    (void)ctx;
    return config_domain_wifi_list_parameters(param_names, max_params, count);
}

static esp_err_t wifi_ns_get_param_info(void* ctx, const char* param_name, config_param_info_t* info) {
    (void)ctx;
    return config_domain_wifi_get_param_info(param_name, info);
}

static esp_err_t wifi_ns_get_uint32(void* ctx, const char* param_name, uint32_t* value) {
    config_wifi_namespace_context_t* c = wifi_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_wifi_get_uint32(c->config, param_name, value);
}

static esp_err_t wifi_ns_set_uint32(void* ctx, const char* param_name, uint32_t value) {
    config_wifi_namespace_context_t* c = wifi_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_wifi_set_uint32(c->config, param_name, value);
}

static esp_err_t wifi_ns_get_string(void* ctx, const char* param_name, char* value, size_t max_len) {
    config_wifi_namespace_context_t* c = wifi_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_wifi_get_string(c->config, param_name, value, max_len);
}

static esp_err_t wifi_ns_set_string(void* ctx, const char* param_name, const char* value) {
    config_wifi_namespace_context_t* c = wifi_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_wifi_set_string(c->config, param_name, value);
}

const config_namespace_descriptor_t g_wifi_namespace_descriptor = {
    .ns_id = CONFIG_NS_WIFI,
    .ns_name = "wifi",
    .load_defaults = wifi_ns_load_defaults,
    .load_from_nvs = wifi_ns_load_from_nvs,
    .write_defaults_to_nvs = wifi_ns_write_defaults_to_nvs,
    .save = wifi_ns_save,
    .list_parameters = wifi_ns_list_parameters,
    .get_parameter_info = wifi_ns_get_param_info,
    .get_raw = NULL,
    .get_bool = NULL,
    .set_bool = NULL,
    .get_int32 = NULL,
    .set_int32 = NULL,
    .get_uint32 = wifi_ns_get_uint32,
    .set_uint32 = wifi_ns_set_uint32,
    .get_float = NULL,
    .set_float = NULL,
    .get_string = wifi_ns_get_string,
    .set_string = wifi_ns_set_string
};
