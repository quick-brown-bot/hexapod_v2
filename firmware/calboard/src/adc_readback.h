// Rail-voltage readback for the 3 servo-branch connectors, via a resistor
// divider into the RP2040 ADC. Used to compute true ground-truth current
// (I = V / R_eff) instead of assuming a fixed rail voltage, since the rail
// sags under load.

#ifndef HEX_CALBOARD_ADC_READBACK_H
#define HEX_CALBOARD_ADC_READBACK_H

#include <stdint.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

void adc_readback_init(void);

// Branch rail voltage in volts (already scaled back up through the divider).
float adc_readback_get_volts(int channel);

#ifdef __cplusplus
}
#endif

#endif // HEX_CALBOARD_ADC_READBACK_H
