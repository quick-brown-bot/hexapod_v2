// Captures the pulse width of an incoming RC-servo-style PWM signal (as the
// LegBoard would send to a real servo) on each of the 3 channel inputs, via
// GPIO edge interrupts.

#ifndef HEX_CALBOARD_PWM_CAPTURE_H
#define HEX_CALBOARD_PWM_CAPTURE_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

void pwm_capture_init(void);

// Last captured high-pulse width in microseconds for a channel. 0 if no
// pulse has been captured yet.
int32_t pwm_capture_get_us(int channel);

// True if a pulse was captured within the last ~50 ms (servo PWM is
// nominally 50 Hz / 20 ms period) — false means the source has gone silent
// (e.g. LegBoard rebooted, or this channel is being driven manually via the
// STEP command instead).
bool pwm_capture_is_fresh(int channel);

#ifdef __cplusplus
}
#endif

#endif // HEX_CALBOARD_PWM_CAPTURE_H
