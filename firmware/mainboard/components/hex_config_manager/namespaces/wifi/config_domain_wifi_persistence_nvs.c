#include "config_domain_persistence_nvs.h"

#include "config_domain_defaults.h"
#include "esp_log.h"

static const char *TAG = "cfg_wifi_nvs";

#define WIFI_KEY_AP_MODE         "ap_mode"
#define WIFI_KEY_AP_PREFIX       "ap_pfx"
#define WIFI_KEY_AP_SSID         "ap_ssid"
#define WIFI_KEY_AP_PASS         "ap_pass"
#define WIFI_KEY_AP_CHANNEL      "ap_ch"
#define WIFI_KEY_AP_MAX_CLIENTS  "ap_maxc"
#define WIFI_KEY_TCP_PORT        "tcp_port"
#define WIFI_KEY_TCP_TIMEOUT     "tcp_to"

esp_err_t config_domain_wifi_write_defaults_to_nvs(nvs_handle_t handle) {
    wifi_config_namespace_t defaults;

    config_load_wifi_defaults(&defaults);

    ESP_LOGI(TAG, "Initializing wifi namespace defaults to NVS");

    ESP_ERROR_CHECK(nvs_set_u8(handle, WIFI_KEY_AP_MODE, (uint8_t)defaults.ap_ssid_mode));
    ESP_ERROR_CHECK(nvs_set_str(handle, WIFI_KEY_AP_PREFIX, defaults.ap_fixed_prefix));
    ESP_ERROR_CHECK(nvs_set_str(handle, WIFI_KEY_AP_SSID, defaults.ap_fixed_ssid));
    ESP_ERROR_CHECK(nvs_set_str(handle, WIFI_KEY_AP_PASS, defaults.ap_password));
    ESP_ERROR_CHECK(nvs_set_u8(handle, WIFI_KEY_AP_CHANNEL, (uint8_t)defaults.ap_channel));
    ESP_ERROR_CHECK(nvs_set_u8(handle, WIFI_KEY_AP_MAX_CLIENTS, (uint8_t)defaults.ap_max_clients));
    ESP_ERROR_CHECK(nvs_set_u16(handle, WIFI_KEY_TCP_PORT, (uint16_t)defaults.tcp_listen_port));
    ESP_ERROR_CHECK(nvs_set_u32(handle, WIFI_KEY_TCP_TIMEOUT, defaults.tcp_connection_timeout_ms));

    ESP_ERROR_CHECK(nvs_commit(handle));
    ESP_LOGI(TAG, "WiFi defaults written to NVS");

    return ESP_OK;
}

esp_err_t config_domain_wifi_load_from_nvs(
    nvs_handle_t handle,
    wifi_config_namespace_t* config
) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err;
    size_t required_size = 0;
    uint8_t u8_value = 0;
    uint16_t u16_value = 0;

    ESP_LOGI(TAG, "Loading wifi configuration from NVS (read-only)");

    err = nvs_get_u8(handle, WIFI_KEY_AP_MODE, &u8_value);
    if (err == ESP_OK) {
        config->ap_ssid_mode = (uint32_t)u8_value;
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load ap_ssid_mode: %s", esp_err_to_name(err));
    }

    required_size = sizeof(config->ap_fixed_prefix);
    err = nvs_get_str(handle, WIFI_KEY_AP_PREFIX, config->ap_fixed_prefix, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load ap_fixed_prefix: %s", esp_err_to_name(err));
    }

    required_size = sizeof(config->ap_fixed_ssid);
    err = nvs_get_str(handle, WIFI_KEY_AP_SSID, config->ap_fixed_ssid, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load ap_fixed_ssid: %s", esp_err_to_name(err));
    }

    required_size = sizeof(config->ap_password);
    err = nvs_get_str(handle, WIFI_KEY_AP_PASS, config->ap_password, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load ap_password: %s", esp_err_to_name(err));
    }

    err = nvs_get_u8(handle, WIFI_KEY_AP_CHANNEL, &u8_value);
    if (err == ESP_OK) {
        config->ap_channel = (uint32_t)u8_value;
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load ap_channel: %s", esp_err_to_name(err));
    }

    err = nvs_get_u8(handle, WIFI_KEY_AP_MAX_CLIENTS, &u8_value);
    if (err == ESP_OK) {
        config->ap_max_clients = (uint32_t)u8_value;
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load ap_max_clients: %s", esp_err_to_name(err));
    }

    err = nvs_get_u16(handle, WIFI_KEY_TCP_PORT, &u16_value);
    if (err == ESP_OK) {
        config->tcp_listen_port = (uint32_t)u16_value;
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load tcp_listen_port: %s", esp_err_to_name(err));
    }

    err = nvs_get_u32(handle, WIFI_KEY_TCP_TIMEOUT, &config->tcp_connection_timeout_ms);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to load tcp_connection_timeout_ms: %s", esp_err_to_name(err));
    }

    return ESP_OK;
}

esp_err_t config_domain_wifi_save_to_nvs(
    nvs_handle_t handle,
    const wifi_config_namespace_t* config
) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_ERROR_CHECK(nvs_set_u8(handle, WIFI_KEY_AP_MODE, (uint8_t)config->ap_ssid_mode));
    ESP_ERROR_CHECK(nvs_set_str(handle, WIFI_KEY_AP_PREFIX, config->ap_fixed_prefix));
    ESP_ERROR_CHECK(nvs_set_str(handle, WIFI_KEY_AP_SSID, config->ap_fixed_ssid));
    ESP_ERROR_CHECK(nvs_set_str(handle, WIFI_KEY_AP_PASS, config->ap_password));
    ESP_ERROR_CHECK(nvs_set_u8(handle, WIFI_KEY_AP_CHANNEL, (uint8_t)config->ap_channel));
    ESP_ERROR_CHECK(nvs_set_u8(handle, WIFI_KEY_AP_MAX_CLIENTS, (uint8_t)config->ap_max_clients));
    ESP_ERROR_CHECK(nvs_set_u16(handle, WIFI_KEY_TCP_PORT, (uint16_t)config->tcp_listen_port));
    ESP_ERROR_CHECK(nvs_set_u32(handle, WIFI_KEY_TCP_TIMEOUT, config->tcp_connection_timeout_ms));

    return nvs_commit(handle);
}
