/*
 * NVS storage backend for configuration manager.
 *
 * Isolates NVS partition lifecycle and namespace handle management
 * from higher-level configuration and domain logic.
 */

#pragma once

#include <stddef.h>
#include "esp_err.h"
#include "nvs_flash.h"

#ifdef __cplusplus
extern "C" {
#endif

const char* config_storage_robot_partition_name(void);
const char* config_storage_wifi_partition_name(void);
const char* config_storage_wifi_namespace_name(void);

esp_err_t config_storage_init_default_partition(void);
esp_err_t config_storage_init_partition(const char* partition_name);
esp_err_t config_storage_verify_partition_exists(const char* partition_name);

esp_err_t config_storage_open_namespace_handles(
    const char* partition_name,
    const char* const* namespace_names,
    size_t namespace_count,
    nvs_handle_t* handles_out
);

void config_storage_close_namespace_handles(nvs_handle_t* handles, size_t namespace_count);

esp_err_t config_storage_read_wifi_credentials(
    char* ssid,
    size_t ssid_len,
    char* password,
    size_t password_len
);

#ifdef __cplusplus
}
#endif
