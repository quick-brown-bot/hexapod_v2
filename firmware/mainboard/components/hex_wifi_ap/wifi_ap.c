 #include "wifi_ap.h"
 #include "esp_wifi.h"
 #include "esp_event.h"
 #include "esp_log.h"
 #include "esp_netif.h"
 #include "nvs_flash.h"
 #include "esp_system.h"
 #include "esp_random.h"
 #include <string.h>
 #include <stdio.h>
 #include <stdlib.h>

static const char *TAG = "wifi_ap";
static bool g_started = false;
static char g_active_ssid[33]; // 32 char max + null

static bool build_ssid(char *out, size_t out_sz, const wifi_ap_options_t *opt) {
    if (!out || out_sz == 0) return false;
    out[0] = '\0';
    if (!opt) {
        ESP_LOGE(TAG, "wifi_ap_init_with_options requires non-NULL options");
        return false;
    }
    switch (opt->mode) {
        case WIFI_AP_SSID_FIXED:
            if (!opt->fixed_ssid || opt->fixed_ssid[0] == '\0') {
                ESP_LOGE(TAG, "WIFI_AP_SSID_FIXED requires non-empty fixed_ssid");
                return false;
            }
            snprintf(out, out_sz, "%s", opt->fixed_ssid);
            break;
        case WIFI_AP_SSID_MAC_SUFFIX: {
            if (!opt->fixed_prefix || opt->fixed_prefix[0] == '\0') {
                ESP_LOGE(TAG, "WIFI_AP_SSID_MAC_SUFFIX requires non-empty fixed_prefix");
                return false;
            }
            uint8_t mac[6];
            esp_wifi_get_mac(WIFI_IF_AP, mac); // ignore error, mac defaults to zeros
            const char *pfx = opt->fixed_prefix;
            snprintf(out, out_sz, "%s_%02X%02X%02X", pfx, mac[3], mac[4], mac[5]);
            break; }
        case WIFI_AP_SSID_RANDOM_SUFFIX: {
            if (!opt->fixed_prefix || opt->fixed_prefix[0] == '\0') {
                ESP_LOGE(TAG, "WIFI_AP_SSID_RANDOM_SUFFIX requires non-empty fixed_prefix");
                return false;
            }
            const char *pfx = opt->fixed_prefix;
            uint32_t r = esp_random();
            snprintf(out, out_sz, "%s_%04X", pfx, (unsigned)(r & 0xFFFF));
            break; }
        default:
            ESP_LOGE(TAG, "Invalid WiFi AP SSID mode: %d", (int)opt->mode);
            return false;
    }

    return (out[0] != '\0');
}

bool wifi_ap_init_with_options(const wifi_ap_options_t *opt) {
    if (g_started) return true;
    if (!opt) {
        ESP_LOGE(TAG, "WiFi AP options are required (namespace-backed config only)");
        return false;
    }
    if (opt->channel == 0 || opt->max_clients == 0) {
        ESP_LOGE(TAG, "Invalid WiFi AP options: channel=%u max_clients=%u", (unsigned)opt->channel, (unsigned)opt->max_clients);
        return false;
    }

    esp_err_t err;
    // Init NVS (needed by WiFi driver). Ignore errors if already inited.
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    
    // Initialize network interface layer
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Create default WiFi AP network interface
    esp_netif_create_default_wifi_ap();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    wifi_config_t ap_cfg = {0};
    if (!build_ssid(g_active_ssid, sizeof(g_active_ssid), opt)) {
        ESP_LOGE(TAG, "Failed to build SSID from WiFi namespace configuration");
        return false;
    }
    strncpy((char*)ap_cfg.ap.ssid, g_active_ssid, sizeof(ap_cfg.ap.ssid) - 1);
    ap_cfg.ap.ssid[sizeof(ap_cfg.ap.ssid) - 1] = '\0'; // Ensure null-termination
    ap_cfg.ap.ssid_len = 0; // null-terminated
    const char *pass = opt->password;
    if (pass) {
        snprintf((char*)ap_cfg.ap.password, sizeof(ap_cfg.ap.password), "%s", pass);
    } else {
        ap_cfg.ap.password[0] = '\0';
    }
    ap_cfg.ap.channel = opt->channel;
    ap_cfg.ap.max_connection = opt->max_clients;
    ap_cfg.ap.authmode = (pass && strlen(pass) > 0) ? WIFI_AUTH_WPA_WPA2_PSK : WIFI_AUTH_OPEN;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "AP started SSID='%s' PASS='%s'", g_active_ssid, pass ? pass : "<open>");
    g_started = true;
    return true;
}

const char *wifi_ap_get_ssid(void) {
    return g_started ? g_active_ssid : NULL;
}
