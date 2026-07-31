// Flash-backed persistence for the per-leg identity.
//
// The leg address (1..6) is the one value that must survive reboot and differ
// between otherwise-identical boards. It is written during pre-mount
// calibration over USB. Runtime parameters (MOVE_DURATION, etc.) are NOT
// persisted — they default in firmware and the ESP32 re-applies them on
// recovery (see RS485_PROTOCOL.md "simplest first").

#ifndef HEX_LEG_PERSIST_H
#define HEX_LEG_PERSIST_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Load persisted state from flash; falls back to defaults if absent/invalid.
void persist_init(void);

uint8_t persist_get_address(void);

// Set and persist the leg address. Returns false for an out-of-range address.
bool persist_set_address(uint8_t addr);

#ifdef __cplusplus
}
#endif

#endif // HEX_LEG_PERSIST_H
