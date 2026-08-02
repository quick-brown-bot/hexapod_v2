// Binary-weighted resistor ladder driven through two daisy-chained 74HC595
// shift registers (see config.h for pin/value assignments). One 4-bit "step"
// (0-15) per channel selects which of that channel's 4 resistor legs are
// switched to GND, presenting a known load on the servo connector.

#ifndef HEX_CALBOARD_LADDER_H
#define HEX_CALBOARD_LADDER_H

#include <stdint.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

void ladder_init(void);

// Select step 0-15 for one channel (0=all legs off) and shift it out to the
// registers. Steps for all channels are re-latched together on RCLK.
void ladder_set_step(int channel, uint8_t step);

uint8_t ladder_get_step(int channel);

// Effective resistance (ohms) of the parallel combination of legs active at
// the given step, for computing ground-truth current from measured voltage.
// Returns a very large value (no load) for step 0.
float ladder_effective_ohms(uint8_t step);

#ifdef __cplusplus
}
#endif

#endif // HEX_CALBOARD_LADDER_H
