// Measures the actual achieved loop()/control-step rate on real hardware --
// main.cpp's CONTROL_RATE_HZ gate (config.h) is a software poll against
// micros(), not a hardware timer, so the true rate can only be confirmed by
// measuring it. See calib.cpp's LOOPSTAT? command.

#ifndef HEX_LEG_LOOPSTAT_H
#define HEX_LEG_LOOPSTAT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void loopstat_init(void);

// Call once per loop() iteration / once per executed control step.
void loopstat_tick_loop(void);
void loopstat_tick_control(void);

// Counts from the most recently *completed* 1-second window (updates once
// per second, not live) -- both in Hz.
uint32_t loopstat_get_loop_hz(void);
uint32_t loopstat_get_control_hz(void);

#ifdef __cplusplus
}
#endif

#endif // HEX_LEG_LOOPSTAT_H
