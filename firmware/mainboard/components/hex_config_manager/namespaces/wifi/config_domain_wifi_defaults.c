#include "config_domain_defaults.h"

#include <string.h>

void config_load_wifi_defaults(wifi_config_namespace_t* config) {
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(wifi_config_namespace_t));

    config->ap_ssid_mode = 1; // WIFI_AP_SSID_MAC_SUFFIX in wifi_ap.h
    strcpy(config->ap_fixed_prefix, "HEXAPOD");
    strcpy(config->ap_fixed_ssid, "HEXAPOD_AP");
    strcpy(config->ap_password, "HEXAPOD_ESP32");
    config->ap_channel = 1;
    config->ap_max_clients = 4;
    config->tcp_listen_port = 5555;
    config->tcp_connection_timeout_ms = 60000;
}
