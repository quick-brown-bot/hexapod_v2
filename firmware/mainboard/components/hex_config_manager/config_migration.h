/*
 * Configuration migration helpers.
 *
 * Isolates schema-version read/write and ordered migration execution
 * from domain-specific migration step implementations.
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "nvs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef esp_err_t (*config_migration_step_fn_t)(uint16_t from, uint16_t to);

esp_err_t config_migration_read_global_version(
    nvs_handle_t handle,
    const char* version_key,
    uint16_t* version
);

esp_err_t config_migration_save_global_version(
    nvs_handle_t handle,
    const char* version_key,
    uint16_t version
);

esp_err_t config_migration_run(
    uint16_t from_version,
    uint16_t to_version,
    config_migration_step_fn_t step_fn
);

#ifdef __cplusplus
}
#endif
