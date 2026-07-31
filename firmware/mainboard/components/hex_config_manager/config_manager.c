/*
 * Configuration Manager Implementation
 * 
 * NVS-based configuration persistence with dual-method API.
 * 
 * License: Apache-2.0
 */

#include "config_manager_runtime_api.h"
#include "config_manager_param_api.h"
#include "config_migration.h"
#include "config_namespace_catalog.h"
#include "config_namespace_registry.h"
#include "controller_driver_types.h"
#include "config_storage_nvs.h"
#include "esp_log.h"
#include <stddef.h>

static const char *TAG = "config_mgr";

// Configuration version for migration support
#define CONFIG_SCHEMA_VERSION 3

// Global configuration version key (stored in system namespace for now)
#define GLOBAL_CONFIG_VERSION_KEY "global_ver"  // Max 15 chars for NVS

static esp_err_t ensure_namespace_registry_initialized(void);

// =============================================================================
// Global State
// =============================================================================

// Global manager state
static config_manager_state_t g_manager_state = {0};

// =============================================================================
// Migration System
// =============================================================================

static esp_err_t migrate_v0_to_v1(void) {
    ESP_LOGI(TAG, "Migrating v0 -> v1: Initializing fresh configuration");

    esp_err_t err = ensure_namespace_registry_initialized();
    if (err != ESP_OK) {
        return err;
    }

    size_t ns_count = config_namespace_registry_count();
    for (size_t i = 0; i < ns_count; i++) {
        const config_namespace_registration_t* reg = config_namespace_registry_get_at(i);
        if (!reg || !reg->descriptor || !reg->descriptor->write_defaults_to_nvs) {
            continue;
        }

        err = reg->descriptor->write_defaults_to_nvs(reg->context);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize defaults for namespace %s during migration: %s",
                     reg->descriptor->ns_name,
                     esp_err_to_name(err));
            return err;
        }
    }
    
    ESP_LOGI(TAG, "Migration v0 -> v1 completed");
    return ESP_OK;
}

static esp_err_t migrate_v1_to_v2(void) {
    ESP_LOGI(TAG, "Migrating v1 -> v2: Initializing servo map namespace defaults");

    esp_err_t err = ensure_namespace_registry_initialized();
    if (err != ESP_OK) {
        return err;
    }

    const config_namespace_registration_t* reg = config_namespace_registry_find_by_id(CONFIG_NS_SERVO_MAP);
    if (!reg || !reg->descriptor || !reg->descriptor->write_defaults_to_nvs) {
        ESP_LOGE(TAG, "Servo map namespace descriptor unavailable during migration");
        return ESP_ERR_INVALID_STATE;
    }

    err = reg->descriptor->write_defaults_to_nvs(reg->context);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize defaults for namespace %s during migration: %s",
                 reg->descriptor->ns_name,
                 esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Migration v1 -> v2 completed");
    return ESP_OK;
}

static esp_err_t migrate_v2_to_v3(void) {
    ESP_LOGI(TAG, "Migrating v2 -> v3: Initializing gait namespace defaults");

    esp_err_t err = ensure_namespace_registry_initialized();
    if (err != ESP_OK) {
        return err;
    }

    const config_namespace_registration_t* reg = config_namespace_registry_find_by_id(CONFIG_NS_GAIT);
    if (!reg || !reg->descriptor || !reg->descriptor->write_defaults_to_nvs) {
        ESP_LOGE(TAG, "Gait namespace descriptor unavailable during migration");
        return ESP_ERR_INVALID_STATE;
    }

    err = reg->descriptor->write_defaults_to_nvs(reg->context);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize defaults for namespace %s during migration: %s",
                 reg->descriptor->ns_name,
                 esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Migration v2 -> v3 completed");
    return ESP_OK;
}

static esp_err_t config_migrate_version(uint16_t from, uint16_t to) {
    ESP_LOGI(TAG, "Migrating configuration schema: v%d -> v%d", from, to);
    
    switch (from) {
        case 0:
            if (to == 1) {
                return migrate_v0_to_v1();
            }
            break;
            
        case 1:
            if (to == 2) {
                return migrate_v1_to_v2();
            }
            break;

        case 2:
            if (to == 3) {
                return migrate_v2_to_v3();
            }
            break;
            
        default:
            ESP_LOGE(TAG, "Unknown migration path: v%d -> v%d", from, to);
            return ESP_ERR_NOT_SUPPORTED;
    }
    
    return ESP_ERR_NOT_SUPPORTED;
}

// =============================================================================
// Helper Functions  
// =============================================================================

static bool g_namespace_registry_initialized = false;

static esp_err_t build_registered_namespace_name_table(const char* namespace_names[CONFIG_NS_COUNT]) {
    if (ensure_namespace_registry_initialized() != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }

    for (size_t i = 0; i < CONFIG_NS_COUNT; i++) {
        namespace_names[i] = NULL;
    }

    size_t ns_count = config_namespace_registry_count();
    for (size_t i = 0; i < ns_count; i++) {
        const config_namespace_registration_t* reg = config_namespace_registry_get_at(i);
        if (!reg || !reg->descriptor) {
            continue;
        }

        config_namespace_t ns_id = reg->descriptor->ns_id;
        if (ns_id < 0 || ns_id >= CONFIG_NS_COUNT) {
            return ESP_ERR_INVALID_STATE;
        }

        namespace_names[ns_id] = reg->descriptor->ns_name;
    }

    for (size_t i = 0; i < CONFIG_NS_COUNT; i++) {
        if (!namespace_names[i]) {
            return ESP_ERR_INVALID_STATE;
        }
    }

    return ESP_OK;
}

static esp_err_t ensure_namespace_registry_initialized(void) {
    if (g_namespace_registry_initialized) {
        return ESP_OK;
    }

    esp_err_t err = config_namespace_catalog_register_all(
        &g_manager_state,
        CONFIG_SCHEMA_VERSION,
        CONTROLLER_DRIVER_FLYSKY_IBUS
    );
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register namespace catalog: %s", esp_err_to_name(err));
        return err;
    }

    g_namespace_registry_initialized = true;
    return ESP_OK;
}

// =============================================================================
// Core Configuration Manager API
// =============================================================================

esp_err_t config_manager_init(void) {
    esp_err_t err;
    const char* robot_partition = config_storage_robot_partition_name();
    bool handles_opened = false;
    const char* namespace_names[CONFIG_NS_COUNT] = {0};

    err = ensure_namespace_registry_initialized();
    if (err != ESP_OK) {
        return err;
    }
    
    if (g_manager_state.initialized) {
        ESP_LOGW(TAG, "Configuration manager already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing configuration manager");
    
    // Initialize default NVS partition (for WiFi)
    err = config_storage_init_default_partition();
    ESP_ERROR_CHECK(err);
    
    // Initialize robot NVS partition
    err = config_storage_init_partition(robot_partition);
    ESP_ERROR_CHECK(err);
    
    ESP_LOGI(TAG, "Both NVS partitions initialized successfully");
    
    // Verify robot partition exists
    err = config_storage_verify_partition_exists(robot_partition);
    if (err != ESP_OK) {
        return err;
    }

    // Open NVS handles for all namespaces in the robot partition.
    err = build_registered_namespace_name_table(namespace_names);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to build namespace name table: %s", esp_err_to_name(err));
        return err;
    }

    err = config_storage_open_namespace_handles(
        robot_partition,
        (const char* const*)namespace_names,
        CONFIG_NS_COUNT,
        g_manager_state.nvs_handles
    );
    if (err != ESP_OK) {
        return err;
    }
    handles_opened = true;
    
    // STEP 1: Read global configuration version
    uint16_t stored_version = 0;
    err = config_migration_read_global_version(
        g_manager_state.nvs_handles[CONFIG_NS_SYSTEM],
        GLOBAL_CONFIG_VERSION_KEY,
        &stored_version
    );
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read global config version: %s", esp_err_to_name(err));
        goto fail;
    }
    
    ESP_LOGI(TAG, "Global config version: stored=%d, current=%d", stored_version, CONFIG_SCHEMA_VERSION);
    
    // STEP 2: Run migration FIRST if needed (before any loading)
    if (stored_version != CONFIG_SCHEMA_VERSION) {
        ESP_LOGI(TAG, "Configuration migration required");
        err = config_migration_run(stored_version, CONFIG_SCHEMA_VERSION, config_migrate_version);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Migration failed: %s", esp_err_to_name(err));
            goto fail;
        }
        
        // Save updated global version
        err = config_migration_save_global_version(
            g_manager_state.nvs_handles[CONFIG_NS_SYSTEM],
            GLOBAL_CONFIG_VERSION_KEY,
            CONFIG_SCHEMA_VERSION
        );
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save new global version: %s", esp_err_to_name(err));
            goto fail;
        }
        
        ESP_LOGI(TAG, "Configuration migration completed successfully");
    } else {
        ESP_LOGI(TAG, "No migration needed - configuration up to date");
    }
    
    size_t ns_count = config_namespace_registry_count();

    // STEP 3: Load namespace defaults into memory cache
    for (size_t i = 0; i < ns_count; i++) {
        const config_namespace_registration_t* reg = config_namespace_registry_get_at(i);
        if (!reg || !reg->descriptor || !reg->descriptor->load_defaults) {
            continue;
        }

        err = reg->descriptor->load_defaults(reg->context);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to load defaults for namespace %s: %s",
                     reg->descriptor->ns_name,
                     esp_err_to_name(err));
            goto fail;
        }
    }

    // STEP 4: Load namespace data from NVS (guaranteed correct schema now)
    for (size_t i = 0; i < ns_count; i++) {
        const config_namespace_registration_t* reg = config_namespace_registry_get_at(i);
        if (!reg || !reg->descriptor || !reg->descriptor->load_from_nvs) {
            continue;
        }

        err = reg->descriptor->load_from_nvs(reg->context);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to load namespace %s from NVS: %s",
                     reg->descriptor->ns_name,
                     esp_err_to_name(err));
            goto fail;
        }
    }
    
    g_manager_state.initialized = true;
    ESP_LOGI(TAG, "Configuration manager initialized successfully");
    
    return ESP_OK;

fail:
    if (handles_opened) {
        config_storage_close_namespace_handles(g_manager_state.nvs_handles, CONFIG_NS_COUNT);
    }
    return err;
}

esp_err_t config_manager_get_state(config_manager_state_t *state) {
    if (!state) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *state = g_manager_state;
    return ESP_OK;
}

bool config_manager_has_dirty_data(void) {
    if (ensure_namespace_registry_initialized() != ESP_OK) {
        return false;
    }

    size_t ns_count = config_namespace_registry_count();
    for (size_t i = 0; i < ns_count; i++) {
        const config_namespace_registration_t* reg = config_namespace_registry_get_at(i);
        if (!reg || !reg->descriptor) {
            continue;
        }

        config_namespace_t ns_id = reg->descriptor->ns_id;
        if (ns_id >= 0 && ns_id < CONFIG_NS_COUNT && g_manager_state.namespace_dirty[ns_id]) {
            return true;
        }
    }

    return false;
}

esp_err_t config_manager_save_namespace(config_namespace_t ns) {
    if (!g_manager_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    const config_namespace_registration_t* reg = config_namespace_registry_find_by_id(ns);
    if (!reg || !reg->descriptor) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!reg->descriptor->save) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    ESP_LOGI(TAG, "Saving namespace: %s", reg->descriptor->ns_name);
    return reg->descriptor->save(reg->context);
}

esp_err_t config_manager_save_namespace_by_name(const char* namespace_str) {
    if (!namespace_str) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!g_manager_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = ensure_namespace_registry_initialized();
    if (err != ESP_OK) {
        return err;
    }

    const config_namespace_registration_t* reg = config_namespace_registry_find_by_name(namespace_str);
    if (!reg || !reg->descriptor) {
        return ESP_ERR_NOT_FOUND;
    }
    if (!reg->descriptor->save) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    ESP_LOGI(TAG, "Saving namespace: %s", reg->descriptor->ns_name);
    return reg->descriptor->save(reg->context);
}

// =============================================================================
// Hybrid Parameter API (Approach B)
// =============================================================================

static const config_namespace_registration_t* find_namespace_registration(const char* namespace_str) {
    if (!namespace_str) {
        return NULL;
    }

    if (ensure_namespace_registry_initialized() != ESP_OK) {
        return NULL;
    }

    return config_namespace_registry_find_by_name(namespace_str);
}

esp_err_t hexapod_config_get_bool(const char* namespace_str, const char* param_name, bool* value) {
    if (!namespace_str || !param_name || !value) {
        return ESP_ERR_INVALID_ARG;
    }

    const config_namespace_registration_t* reg = find_namespace_registration(namespace_str);
    if (!reg || !reg->descriptor || !reg->descriptor->get_bool) {
        return ESP_ERR_NOT_FOUND;
    }

    return reg->descriptor->get_bool(reg->context, param_name, value);
}

esp_err_t hexapod_config_set_bool(const char* namespace_str, const char* param_name, bool value, bool persist) {
    if (!namespace_str || !param_name) {
        return ESP_ERR_INVALID_ARG;
    }

    const config_namespace_registration_t* reg = find_namespace_registration(namespace_str);
    if (!reg || !reg->descriptor || !reg->descriptor->set_bool) {
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t err = reg->descriptor->set_bool(reg->context, param_name, value);
    if (err != ESP_OK) {
        return err;
    }

    g_manager_state.namespace_dirty[reg->descriptor->ns_id] = true;
    ESP_LOGD(TAG, "Set %s.%s = %s %s", namespace_str, param_name,
             value ? "true" : "false", persist ? "(persistent)" : "(memory-only)");

    if (persist) {
        return config_manager_save_namespace(reg->descriptor->ns_id);
    }

    return ESP_OK;
}

esp_err_t hexapod_config_get_int32(const char* namespace_str, const char* param_name, int32_t* value) {
    if (!namespace_str || !param_name || !value) {
        return ESP_ERR_INVALID_ARG;
    }

    const config_namespace_registration_t* reg = find_namespace_registration(namespace_str);
    if (!reg || !reg->descriptor || !reg->descriptor->get_int32) {
        return ESP_ERR_NOT_FOUND;
    }

    return reg->descriptor->get_int32(reg->context, param_name, value);
}

esp_err_t hexapod_config_set_int32(const char* namespace_str, const char* param_name, int32_t value, bool persist) {
    if (!namespace_str || !param_name) {
        return ESP_ERR_INVALID_ARG;
    }

    const config_namespace_registration_t* reg = find_namespace_registration(namespace_str);
    if (!reg || !reg->descriptor || !reg->descriptor->set_int32) {
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t err = reg->descriptor->set_int32(reg->context, param_name, value);
    if (err != ESP_OK) {
        return err;
    }

    g_manager_state.namespace_dirty[reg->descriptor->ns_id] = true;
    ESP_LOGD(TAG, "Set %s.%s = %ld %s", namespace_str, param_name,
             (long)value, persist ? "(persistent)" : "(memory-only)");

    if (persist) {
        return config_manager_save_namespace(reg->descriptor->ns_id);
    }

    return ESP_OK;
}

esp_err_t hexapod_config_get_uint32(const char* namespace_str, const char* param_name, uint32_t* value) {
    if (!namespace_str || !param_name || !value) {
        return ESP_ERR_INVALID_ARG;
    }

    const config_namespace_registration_t* reg = find_namespace_registration(namespace_str);
    if (!reg || !reg->descriptor || !reg->descriptor->get_uint32) {
        return ESP_ERR_NOT_FOUND;
    }

    return reg->descriptor->get_uint32(reg->context, param_name, value);
}

esp_err_t hexapod_config_set_uint32(const char* namespace_str, const char* param_name, uint32_t value, bool persist) {
    if (!namespace_str || !param_name) {
        return ESP_ERR_INVALID_ARG;
    }

    const config_namespace_registration_t* reg = find_namespace_registration(namespace_str);
    if (!reg || !reg->descriptor || !reg->descriptor->set_uint32) {
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t err = reg->descriptor->set_uint32(reg->context, param_name, value);
    if (err != ESP_OK) {
        return err;
    }

    g_manager_state.namespace_dirty[reg->descriptor->ns_id] = true;
    ESP_LOGD(TAG, "Set %s.%s = %lu %s", namespace_str, param_name,
             (unsigned long)value, persist ? "(persistent)" : "(memory-only)");

    if (persist) {
        return config_manager_save_namespace(reg->descriptor->ns_id);
    }

    return ESP_OK;
}

esp_err_t hexapod_config_get_float(const char* namespace_str, const char* param_name, float* value) {
    if (!namespace_str || !param_name || !value) {
        return ESP_ERR_INVALID_ARG;
    }

    const config_namespace_registration_t* reg = find_namespace_registration(namespace_str);
    if (!reg || !reg->descriptor || !reg->descriptor->get_float) {
        return ESP_ERR_NOT_FOUND;
    }

    return reg->descriptor->get_float(reg->context, param_name, value);
}

esp_err_t hexapod_config_set_float(const char* namespace_str, const char* param_name, float value, bool persist) {
    if (!namespace_str || !param_name) {
        return ESP_ERR_INVALID_ARG;
    }

    const config_namespace_registration_t* reg = find_namespace_registration(namespace_str);
    if (!reg || !reg->descriptor || !reg->descriptor->set_float) {
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t err = reg->descriptor->set_float(reg->context, param_name, value);
    if (err != ESP_OK) {
        return err;
    }

    g_manager_state.namespace_dirty[reg->descriptor->ns_id] = true;
    ESP_LOGD(TAG, "Set %s.%s = %.3f %s", namespace_str, param_name,
             value, persist ? "(persistent)" : "(memory-only)");

    if (persist) {
        return config_manager_save_namespace(reg->descriptor->ns_id);
    }

    return ESP_OK;
}

esp_err_t hexapod_config_get_string(const char* namespace_str, const char* param_name, char* value, size_t max_len) {
    if (!namespace_str || !param_name || !value || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const config_namespace_registration_t* reg = find_namespace_registration(namespace_str);
    if (!reg || !reg->descriptor || !reg->descriptor->get_string) {
        return ESP_ERR_NOT_FOUND;
    }

    return reg->descriptor->get_string(reg->context, param_name, value, max_len);
}

esp_err_t hexapod_config_set_string(const char* namespace_str, const char* param_name, const char* value, bool persist) {
    if (!namespace_str || !param_name || !value) {
        return ESP_ERR_INVALID_ARG;
    }

    const config_namespace_registration_t* reg = find_namespace_registration(namespace_str);
    if (!reg || !reg->descriptor || !reg->descriptor->set_string) {
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t err = reg->descriptor->set_string(reg->context, param_name, value);
    if (err != ESP_OK) {
        return err;
    }

    g_manager_state.namespace_dirty[reg->descriptor->ns_id] = true;
    ESP_LOGD(TAG, "Set %s.%s = \"%s\" %s", namespace_str, param_name,
             value, persist ? "(persistent)" : "(memory-only)");

    if (persist) {
        return config_manager_save_namespace(reg->descriptor->ns_id);
    }

    return ESP_OK;
}

// =============================================================================
// Parameter Discovery and Metadata API
// =============================================================================

esp_err_t config_list_namespaces(const char** namespace_names, size_t* count) {
    if (!namespace_names || !count) {
        return ESP_ERR_INVALID_ARG;
    }

    if (ensure_namespace_registry_initialized() != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t ns_count = config_namespace_registry_count();
    for (size_t i = 0; i < ns_count; i++) {
        const config_namespace_registration_t* reg = config_namespace_registry_get_at(i);
        if (reg && reg->descriptor) {
            namespace_names[i] = reg->descriptor->ns_name;
        }
    }

    *count = ns_count;
    return ESP_OK;
}

esp_err_t config_list_parameters(const char* namespace_str, const char** param_names, 
                                size_t max_params, size_t* count) {
    if (!namespace_str || !param_names || !count) {
        ESP_LOGE(TAG, "Invalid arguments to config_list_parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    const config_namespace_registration_t* reg = find_namespace_registration(namespace_str);
    if (!reg || !reg->descriptor || !reg->descriptor->list_parameters) {
        return ESP_ERR_NOT_FOUND;
    }

    return reg->descriptor->list_parameters(reg->context, param_names, max_params, count);
}

esp_err_t config_get_parameter_info(const char* namespace_str, const char* param_name, 
                                   config_param_info_t* info) {
    if (!namespace_str || !param_name || !info) {
        return ESP_ERR_INVALID_ARG;
    }
    
    const config_namespace_registration_t* reg = find_namespace_registration(namespace_str);
    if (!reg || !reg->descriptor || !reg->descriptor->get_parameter_info) {
        return ESP_ERR_NOT_FOUND;
    }

    return reg->descriptor->get_parameter_info(reg->context, param_name, info);
}

// =============================================================================
// Configuration Defaults
// =============================================================================

esp_err_t config_factory_reset(void) {
    ESP_LOGW(TAG, "Performing factory reset - robot configuration will be lost!");
    ESP_LOGI(TAG, "Note: WiFi credentials in default partition will be preserved");

    if (ensure_namespace_registry_initialized() != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Clear all robot configuration namespaces (preserves WiFi partition)
    size_t ns_count = config_namespace_registry_count();
    for (size_t i = 0; i < ns_count; i++) {
        const config_namespace_registration_t* reg = config_namespace_registry_get_at(i);
        if (!reg || !reg->descriptor) {
            continue;
        }

        config_namespace_t ns_id = reg->descriptor->ns_id;
        if (ns_id < 0 || ns_id >= CONFIG_NS_COUNT) {
            return ESP_ERR_INVALID_STATE;
        }

        esp_err_t err = nvs_erase_all(g_manager_state.nvs_handles[ns_id]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to erase namespace %s: %s", 
                     reg->descriptor->ns_name, esp_err_to_name(err));
            return err;
        }

        err = nvs_commit(g_manager_state.nvs_handles[ns_id]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to commit erase for namespace %s: %s",
                     reg->descriptor->ns_name, esp_err_to_name(err));
            return err;
        }
    }
    
    // Reload defaults through namespace descriptors
    for (size_t i = 0; i < ns_count; i++) {
        const config_namespace_registration_t* reg = config_namespace_registry_get_at(i);
        if (!reg || !reg->descriptor || !reg->descriptor->load_defaults) {
            continue;
        }

        esp_err_t err = reg->descriptor->load_defaults(reg->context);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to reload defaults for namespace %s: %s",
                     reg->descriptor->ns_name,
                     esp_err_to_name(err));
            return err;
        }
    }
    
    // Clear dirty flags
    for (size_t i = 0; i < ns_count; i++) {
        const config_namespace_registration_t* reg = config_namespace_registry_get_at(i);
        if (!reg || !reg->descriptor) {
            continue;
        }

        config_namespace_t ns_id = reg->descriptor->ns_id;
        if (ns_id >= 0 && ns_id < CONFIG_NS_COUNT) {
            g_manager_state.namespace_dirty[ns_id] = false;
            g_manager_state.namespace_loaded[ns_id] = true;
        }
    }
    
    ESP_LOGI(TAG, "Factory reset completed");
    return ESP_OK;
}

// =============================================================================
// Utility Functions for WiFi Partition Access (if needed)
// =============================================================================

esp_err_t config_get_wifi_credentials(char* ssid, size_t ssid_len, char* password, size_t password_len) {
    return config_storage_read_wifi_credentials(ssid, ssid_len, password, password_len);
}
