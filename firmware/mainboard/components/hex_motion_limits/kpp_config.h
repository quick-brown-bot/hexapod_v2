/*
 * KPP compile-time debug/logging toggles.
 *
 * Runtime motion tuning values are sourced from the motion_lim namespace.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Enable detailed state logging (ESP_LOGD)
#define KPP_ENABLE_STATE_LOGGING   1

// Log interval for periodic state dumps (control cycles)
#define KPP_LOG_INTERVAL           100

// Enable motion limiting debug info
#define KPP_ENABLE_LIMIT_LOGGING   1

#ifdef __cplusplus
}
#endif
