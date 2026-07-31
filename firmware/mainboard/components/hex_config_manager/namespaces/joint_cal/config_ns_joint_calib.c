#include "config_ns_joint_calib.h"

#include "config_domain_defaults.h"
#include "config_domain_joint_access.h"
#include "config_domain_persistence_nvs.h"
#include "config_manager_runtime_api.h"
#include "config_registry.h"

static joint_calib_config_t g_joint_namespace_config = {0};
static config_joint_calib_namespace_context_t g_joint_namespace_context = {0};

void config_joint_namespace_bind(
    nvs_handle_t* nvs_handle,
    bool* namespace_dirty,
    bool* namespace_loaded
) {
    g_joint_namespace_context.nvs_handle = nvs_handle;
    g_joint_namespace_context.namespace_dirty = namespace_dirty;
    g_joint_namespace_context.namespace_loaded = namespace_loaded;
    g_joint_namespace_context.config = &g_joint_namespace_config;
}

void* config_joint_namespace_context(void) {
    return &g_joint_namespace_context;
}

joint_calib_config_t* config_joint_namespace_config(void) {
    return &g_joint_namespace_config;
}

const joint_calib_config_t* config_get_joint_calib(void) {
    return &g_joint_namespace_config;
}

esp_err_t config_set_joint_calib(const joint_calib_config_t* config) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    g_joint_namespace_config = *config;
    if (g_joint_namespace_context.namespace_dirty) {
        *g_joint_namespace_context.namespace_dirty = true;
    }

    esp_err_t err = config_manager_save_namespace(CONFIG_NS_JOINT_CALIB);
    if (err == ESP_OK && g_joint_namespace_context.namespace_dirty) {
        *g_joint_namespace_context.namespace_dirty = false;
    }

    return err;
}

esp_err_t config_get_joint_calib_data(int leg_index, int joint, joint_calib_t* calib_data) {
    if (!calib_data || leg_index < 0 || leg_index >= NUM_LEGS || joint < 0 || joint >= NUM_JOINTS_PER_LEG) {
        return ESP_ERR_INVALID_ARG;
    }

    *calib_data = g_joint_namespace_config.joints[leg_index][joint];
    return ESP_OK;
}

esp_err_t config_set_joint_calib_data_memory(int leg_index, int joint, const joint_calib_t* calib_data) {
    if (!calib_data || leg_index < 0 || leg_index >= NUM_LEGS || joint < 0 || joint >= NUM_JOINTS_PER_LEG) {
        return ESP_ERR_INVALID_ARG;
    }

    g_joint_namespace_config.joints[leg_index][joint] = *calib_data;
    if (g_joint_namespace_context.namespace_dirty) {
        *g_joint_namespace_context.namespace_dirty = true;
    }

    return ESP_OK;
}

esp_err_t config_set_joint_calib_data_persist(int leg_index, int joint, const joint_calib_t* calib_data) {
    esp_err_t err = config_set_joint_calib_data_memory(leg_index, joint, calib_data);
    if (err != ESP_OK) {
        return err;
    }

    err = config_manager_save_namespace(CONFIG_NS_JOINT_CALIB);
    if (err == ESP_OK && g_joint_namespace_context.namespace_dirty) {
        *g_joint_namespace_context.namespace_dirty = false;
    }
    return err;
}

esp_err_t config_set_joint_offset_memory(int leg_index, int joint, float offset_rad) {
    if (leg_index < 0 || leg_index >= NUM_LEGS || joint < 0 || joint >= NUM_JOINTS_PER_LEG) {
        return ESP_ERR_INVALID_ARG;
    }

    g_joint_namespace_config.joints[leg_index][joint].zero_offset_rad = offset_rad;
    if (g_joint_namespace_context.namespace_dirty) {
        *g_joint_namespace_context.namespace_dirty = true;
    }

    return ESP_OK;
}

esp_err_t config_set_joint_offset_persist(int leg_index, int joint, float offset_rad) {
    esp_err_t err = config_set_joint_offset_memory(leg_index, joint, offset_rad);
    if (err != ESP_OK) {
        return err;
    }

    err = config_manager_save_namespace(CONFIG_NS_JOINT_CALIB);
    if (err == ESP_OK && g_joint_namespace_context.namespace_dirty) {
        *g_joint_namespace_context.namespace_dirty = false;
    }
    return err;
}

esp_err_t config_set_joint_limits_memory(int leg_index, int joint, float min_rad, float max_rad) {
    if (leg_index < 0 || leg_index >= NUM_LEGS || joint < 0 || joint >= NUM_JOINTS_PER_LEG) {
        return ESP_ERR_INVALID_ARG;
    }

    g_joint_namespace_config.joints[leg_index][joint].min_rad = min_rad;
    g_joint_namespace_config.joints[leg_index][joint].max_rad = max_rad;
    if (g_joint_namespace_context.namespace_dirty) {
        *g_joint_namespace_context.namespace_dirty = true;
    }

    return ESP_OK;
}

esp_err_t config_set_joint_limits_persist(int leg_index, int joint, float min_rad, float max_rad) {
    esp_err_t err = config_set_joint_limits_memory(leg_index, joint, min_rad, max_rad);
    if (err != ESP_OK) {
        return err;
    }

    err = config_manager_save_namespace(CONFIG_NS_JOINT_CALIB);
    if (err == ESP_OK && g_joint_namespace_context.namespace_dirty) {
        *g_joint_namespace_context.namespace_dirty = false;
    }
    return err;
}

esp_err_t config_set_joint_pwm_memory(int leg_index, int joint, int32_t pwm_min_us, int32_t pwm_max_us, int32_t pwm_neutral_us) {
    if (leg_index < 0 || leg_index >= NUM_LEGS || joint < 0 || joint >= NUM_JOINTS_PER_LEG) {
        return ESP_ERR_INVALID_ARG;
    }

    g_joint_namespace_config.joints[leg_index][joint].pwm_min_us = pwm_min_us;
    g_joint_namespace_config.joints[leg_index][joint].pwm_max_us = pwm_max_us;
    g_joint_namespace_config.joints[leg_index][joint].neutral_us = pwm_neutral_us;
    if (g_joint_namespace_context.namespace_dirty) {
        *g_joint_namespace_context.namespace_dirty = true;
    }

    return ESP_OK;
}

esp_err_t config_set_joint_pwm_persist(int leg_index, int joint, int32_t pwm_min_us, int32_t pwm_max_us, int32_t pwm_neutral_us) {
    esp_err_t err = config_set_joint_pwm_memory(leg_index, joint, pwm_min_us, pwm_max_us, pwm_neutral_us);
    if (err != ESP_OK) {
        return err;
    }

    err = config_manager_save_namespace(CONFIG_NS_JOINT_CALIB);
    if (err == ESP_OK && g_joint_namespace_context.namespace_dirty) {
        *g_joint_namespace_context.namespace_dirty = false;
    }
    return err;
}

static config_joint_calib_namespace_context_t* joint_ctx(void* ctx) {
    return (config_joint_calib_namespace_context_t*)ctx;
}

static esp_err_t joint_ns_load_defaults(void* ctx) {
    config_joint_calib_namespace_context_t* c = joint_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    config_load_joint_calib_defaults(c->config);
    return ESP_OK;
}

static esp_err_t joint_ns_load_from_nvs(void* ctx) {
    config_joint_calib_namespace_context_t* c = joint_ctx(ctx);
    if (!c || !c->nvs_handle || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_domain_joint_cal_load_from_nvs(*c->nvs_handle, c->config);
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

static esp_err_t joint_ns_write_defaults_to_nvs(void* ctx) {
    config_joint_calib_namespace_context_t* c = joint_ctx(ctx);
    if (!c || !c->nvs_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_joint_cal_write_defaults_to_nvs(*c->nvs_handle);
}

static esp_err_t joint_ns_save(void* ctx) {
    config_joint_calib_namespace_context_t* c = joint_ctx(ctx);
    if (!c || !c->nvs_handle || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = config_domain_joint_cal_save_to_nvs(*c->nvs_handle, c->config);
    if (err != ESP_OK) {
        return err;
    }

    if (c->namespace_dirty) {
        *c->namespace_dirty = false;
    }

    return ESP_OK;
}

static esp_err_t joint_ns_list_parameters(void* ctx, const char** param_names, size_t max_params, size_t* count) {
    (void)ctx;
    return config_registry_list_joint_parameters(param_names, max_params, count);
}

static esp_err_t joint_ns_get_param_info(void* ctx, const char* param_name, config_param_info_t* info) {
    (void)ctx;
    int leg_index = 0;
    int joint_index = 0;
    const char* param_suffix = NULL;
    esp_err_t err = config_domain_joint_parse_param_name(param_name, &leg_index, &joint_index, &param_suffix);
    if (err != ESP_OK || !param_suffix) {
        return ESP_ERR_NOT_FOUND;
    }

    return config_registry_build_joint_param_info(leg_index, joint_index, param_suffix, param_name, info);
}

static esp_err_t joint_ns_get_int32(void* ctx, const char* param_name, int32_t* value) {
    config_joint_calib_namespace_context_t* c = joint_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_joint_get_int32(c->config, param_name, value);
}

static esp_err_t joint_ns_set_int32(void* ctx, const char* param_name, int32_t value) {
    config_joint_calib_namespace_context_t* c = joint_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_joint_set_int32(c->config, param_name, value);
}

static esp_err_t joint_ns_get_float(void* ctx, const char* param_name, float* value) {
    config_joint_calib_namespace_context_t* c = joint_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_joint_get_float(c->config, param_name, value);
}

static esp_err_t joint_ns_set_float(void* ctx, const char* param_name, float value) {
    config_joint_calib_namespace_context_t* c = joint_ctx(ctx);
    if (!c || !c->config) {
        return ESP_ERR_INVALID_ARG;
    }

    return config_domain_joint_set_float(c->config, param_name, value);
}

const config_namespace_descriptor_t g_joint_namespace_descriptor = {
    .ns_id = CONFIG_NS_JOINT_CALIB,
    .ns_name = "joint_cal",
    .load_defaults = joint_ns_load_defaults,
    .load_from_nvs = joint_ns_load_from_nvs,
    .write_defaults_to_nvs = joint_ns_write_defaults_to_nvs,
    .save = joint_ns_save,
    .list_parameters = joint_ns_list_parameters,
    .get_parameter_info = joint_ns_get_param_info,
    .get_raw = NULL,
    .get_bool = NULL,
    .set_bool = NULL,
    .get_int32 = joint_ns_get_int32,
    .set_int32 = joint_ns_set_int32,
    .get_uint32 = NULL,
    .set_uint32 = NULL,
    .get_float = joint_ns_get_float,
    .set_float = joint_ns_set_float,
    .get_string = NULL,
    .set_string = NULL
};
