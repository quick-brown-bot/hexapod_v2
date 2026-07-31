#include "config_migration.h"

#include "esp_log.h"
#include "nvs.h"

static const char* TAG = "cfg_migration";

esp_err_t config_migration_read_global_version(
    nvs_handle_t handle,
    const char* version_key,
    uint16_t* version
) {
    if (!version_key || !version) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = nvs_get_u16(handle, version_key, version);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        *version = 0;
        return ESP_OK;
    }

    return err;
}

esp_err_t config_migration_save_global_version(
    nvs_handle_t handle,
    const char* version_key,
    uint16_t version
) {
    if (!version_key) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_ERROR_CHECK(nvs_set_u16(handle, version_key, version));
    ESP_ERROR_CHECK(nvs_commit(handle));
    return ESP_OK;
}

esp_err_t config_migration_run(
    uint16_t from_version,
    uint16_t to_version,
    config_migration_step_fn_t step_fn
) {
    if (!step_fn) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Starting configuration migration: v%d -> v%d", from_version, to_version);

    if (from_version == to_version) {
        ESP_LOGI(TAG, "No migration needed - versions match");
        return ESP_OK;
    }

    if (from_version > to_version) {
        ESP_LOGE(TAG, "Downgrade not supported: v%d -> v%d", from_version, to_version);
        return ESP_ERR_NOT_SUPPORTED;
    }

    for (uint16_t current = from_version; current < to_version; current++) {
        esp_err_t err = step_fn(current, current + 1);
        if (err != ESP_OK) {
            ESP_LOGE(
                TAG,
                "Migration failed at v%d -> v%d: %s",
                current,
                current + 1,
                esp_err_to_name(err)
            );
            return err;
        }
    }

    ESP_LOGI(TAG, "Migration completed successfully");
    return ESP_OK;
}
