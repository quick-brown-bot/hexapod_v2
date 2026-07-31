#include "config_domain_defaults.h"

#include "esp_mac.h"

#include <stdio.h>
#include <string.h>

#define CONFIG_SCHEMA_VERSION_DEFAULT 1

void config_load_system_defaults(system_config_t* config) {
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(system_config_t));

    config->emergency_stop_enabled = true;
    config->auto_disarm_timeout = 30;
    config->safety_voltage_min = 6.5f;
    config->temperature_limit_max = 80.0f;
    config->motion_timeout_ms = 1000;
    config->startup_delay_ms = 2000;
    config->max_control_frequency = 100;

    uint8_t mac[6];
    esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (err == ESP_OK) {
        snprintf(config->robot_id, sizeof(config->robot_id), "HEXAPOD_%02X%02X%02X", mac[3], mac[4], mac[5]);
    } else {
        strcpy(config->robot_id, "HEXAPOD_DEFAULT");
    }

    strcpy(config->robot_name, "My Hexapod Robot");
    config->config_version = CONFIG_SCHEMA_VERSION_DEFAULT;
    config->controller_type = CONTROLLER_DRIVER_FLYSKY_IBUS;
}
