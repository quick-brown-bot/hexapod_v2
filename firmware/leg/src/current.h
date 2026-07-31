// Current sensing — reads the INA4181 4-channel sense outputs via the RP2040 ADC.
// Channel A0=total, A1=coxa, A2=femur, A3=tibia (HARDWARE_AND_MECHANICS.md).

#ifndef HEX_LEG_CURRENT_H
#define HEX_LEG_CURRENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t total_ma;
    uint16_t coxa_ma;
    uint16_t femur_ma;
    uint16_t tibia_ma;
} current_reading_t;

void current_init(void);

// Sample all four channels and store the latest reading internally.
void current_sample(void);

// Copy the most recent sampled reading.
void current_get(current_reading_t *out);

#ifdef __cplusplus
}
#endif

#endif // HEX_LEG_CURRENT_H
