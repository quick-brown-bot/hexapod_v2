// Local trajectory interpolation between received motion targets.
//
// The RP2040 receives sparse joint-angle targets over RS485 (~100 Hz) and
// interpolates between them at the control-loop rate (~1 kHz) so servo motion
// stays smooth. Two modes: LINEAR (bring-up, trivially verifiable) and CUBIC
// (C1-continuous Hermite). "Accept and restart": each new target starts a fresh
// segment from the current interpolated position and velocity, preserving
// continuity. See RS485_PROTOCOL.md (INTERP_MODE, MOVE_DURATION).

#ifndef HEX_LEG_INTERP_H
#define HEX_LEG_INTERP_H

#include <stdint.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

void interp_init(float initial_deg);

// Runtime parameters.
void interp_set_mode(int mode);              // INTERP_MODE_LINEAR / _CUBIC
void interp_set_duration_ms(uint16_t ms);

// Accept a new target for one joint and restart its segment at `now_us`.
void interp_set_target(int joint, float target_deg, uint32_t now_us);

// Advance all joints to `now_us`. Call every control tick.
void interp_update(uint32_t now_us);

// Current interpolated position (degrees) for a joint.
float interp_get_pos(int joint);

// Freeze all joints at their current position (watchdog hold).
void interp_hold(uint32_t now_us);

#ifdef __cplusplus
}
#endif

#endif // HEX_LEG_INTERP_H
