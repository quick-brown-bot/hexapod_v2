#include "config_storage_nvs.h"

#include "esp_log.h"
#include "esp_partition.h"
#include "nvs.h"

static const char* TAG = "cfg_store_nvs";

#define CONFIG_STORAGE_WIFI_PARTITION  "nvs"
#define CONFIG_STORAGE_ROBOT_PARTITION "nvs_robot"
#define CONFIG_STORAGE_WIFI_NAMESPACE  "nvs"

const char* config_storage_robot_partition_name(void) {
    return CONFIG_STORAGE_ROBOT_PARTITION;
}

const char* config_storage_wifi_partition_name(void) {
    return CONFIG_STORAGE_WIFI_PARTITION;
}

const char* config_storage_wifi_namespace_name(void) {
    return CONFIG_STORAGE_WIFI_NAMESPACE;
}

esp_err_t config_storage_init_default_partition(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Default NVS partition needs erase, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

esp_err_t config_storage_init_partition(const char* partition_name) {
    if (!partition_name) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = nvs_flash_init_partition(partition_name);
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition '%s' needs erase, erasing...", partition_name);
        ESP_ERROR_CHECK(nvs_flash_erase_partition(partition_name));
        err = nvs_flash_init_partition(partition_name);
    }
    return err;
}

esp_err_t config_storage_verify_partition_exists(const char* partition_name) {
    if (!partition_name) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t* partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_NVS,
        partition_name
    );
    if (!partition) {
        ESP_LOGE(TAG, "NVS partition '%s' not found in partition table", partition_name);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(
        TAG,
        "Partition found: label='%s', size=%lu bytes, offset=0x%lx",
        partition->label,
        (unsigned long)partition->size,
        (unsigned long)partition->address
    );
    return ESP_OK;
}

esp_err_t config_storage_open_namespace_handles(
    const char* partition_name,
    const char* const* namespace_names,
    size_t namespace_count,
    nvs_handle_t* handles_out
) {
    if (!partition_name || !namespace_names || !handles_out) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < namespace_count; i++) {
        handles_out[i] = 0;
    }

    for (size_t i = 0; i < namespace_count; i++) {
        esp_err_t err = nvs_open_from_partition(
            partition_name,
            namespace_names[i],
            NVS_READWRITE,
            &handles_out[i]
        );
        if (err != ESP_OK) {
            ESP_LOGE(
                TAG,
                "Failed to open namespace '%s' from partition '%s': %s",
                namespace_names[i],
                partition_name,
                esp_err_to_name(err)
            );
            config_storage_close_namespace_handles(handles_out, namespace_count);
            return err;
        }

        ESP_LOGI(
            TAG,
            "Opened namespace '%s' in partition '%s'",
            namespace_names[i],
            partition_name
        );
    }

    return ESP_OK;
}

void config_storage_close_namespace_handles(nvs_handle_t* handles, size_t namespace_count) {
    if (!handles) {
        return;
    }

    for (size_t i = 0; i < namespace_count; i++) {
        if (handles[i] != 0) {
            nvs_close(handles[i]);
            handles[i] = 0;
        }
    }
}

esp_err_t config_storage_read_wifi_credentials(
    char* ssid,
    size_t ssid_len,
    char* password,
    size_t password_len
) {
    if (!ssid || ssid_len == 0 || !password || password_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t wifi_handle;
    esp_err_t err = nvs_open_from_partition(
        config_storage_wifi_partition_name(),
        config_storage_wifi_namespace_name(),
        NVS_READONLY,
        &wifi_handle
    );
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open WiFi NVS partition: %s", esp_err_to_name(err));
        return err;
    }

    size_t required_size = ssid_len;
    err = nvs_get_str(wifi_handle, "wifi.ssid", ssid, &required_size);
    if (err == ESP_OK) {
        required_size = password_len;
        err = nvs_get_str(wifi_handle, "wifi.pswd", password, &required_size);
    }

    nvs_close(wifi_handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGD(TAG, "WiFi credentials not found in NVS");
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error reading WiFi credentials: %s", esp_err_to_name(err));
    }

    return err;
}
