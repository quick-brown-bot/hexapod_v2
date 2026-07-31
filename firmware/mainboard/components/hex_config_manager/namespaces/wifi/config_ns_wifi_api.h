/*
 * Public WiFi namespace types and APIs.
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t ap_ssid_mode;
    char ap_fixed_prefix[32];
    char ap_fixed_ssid[33];
    char ap_password[65];
    uint32_t ap_channel;
    uint32_t ap_max_clients;
    uint32_t tcp_listen_port;
    uint32_t tcp_connection_timeout_ms;
} wifi_config_namespace_t;

const wifi_config_namespace_t* config_get_wifi(void);
esp_err_t config_set_wifi(const wifi_config_namespace_t* config);

void config_load_wifi_defaults(wifi_config_namespace_t* config);

#ifdef __cplusplus
}
#endif
