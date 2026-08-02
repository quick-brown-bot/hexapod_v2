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

// Raw ADC readback for calibration (see tools/current_calibration), one
// entry per channel in ADC_CH_TOTAL/COXA/FEMUR/TIBIA order.
typedef struct {
    uint16_t raw_counts[4];
    float    raw_mv[4];
} current_raw_t;

void current_init(void);

// Sample all four channels and store the latest reading internally. Applies
// each channel's persisted calibration (see persist_get_current_calib()).
void current_sample(void);

// Copy the most recent sampled (calibrated) reading.
void current_get(current_reading_t *out);

// Copy the most recent sampled raw ADC reading (pre-calibration).
void current_get_raw(current_raw_t *out);

// Reload calibration from persistence (call after CURCAL updates it).
void current_reload_calib(void);

#ifdef __cplusplus
}
#endif

#endif // HEX_LEG_CURRENT_H
