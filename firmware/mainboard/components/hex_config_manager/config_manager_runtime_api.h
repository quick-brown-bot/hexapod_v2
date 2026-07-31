/*
 * Core runtime API for configuration manager lifecycle and persistence.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "config_manager_core_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t config_manager_init(void);
esp_err_t config_manager_get_state(config_manager_state_t *state);
bool config_manager_has_dirty_data(void);
esp_err_t config_manager_save_namespace(config_namespace_t ns);
esp_err_t config_manager_save_namespace_by_name(const char* namespace_str);
esp_err_t config_factory_reset(void);

esp_err_t config_get_wifi_credentials(char* ssid, size_t ssid_len, char* password, size_t password_len);

#ifdef __cplusplus
}
#endif
