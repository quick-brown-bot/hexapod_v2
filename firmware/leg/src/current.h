// Current sensing — reads the INA4181 4-channel sense outputs via the RP2040 ADC.
// Physical pins: A0=total, A1=femur, A2=tibia, A3=coxa (NOT alphabetical --
// see config.h's current-sense comment and HARDWARE_AND_MECHANICS.md).
// current.cpp compensates so the logical channel numbering below (also
// CURRAW?/CURCAL's <ch>) stays total=0/coxa=1/femur=2/tibia=3 regardless.

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

// Smoothing applied to the *reported* (calibrated) current -- see
// CURRENT_FILTER_EMA/CURRENT_FILTER_BOXCAR and DEFAULT_CURRENT_EMA_ALPHA in
// config.h. Persisted (persist_get/set_current_filter()) -- unlike
// MOVE_DURATION/WATCHDOG_TIMEOUT/INTERP_MODE, this is a board-level
// current-sensing setting, not something the ESP32 re-applies. Loaded on
// boot, set over USB (calib.cpp CURFILT). raw_mv/raw_counts
// (current_get_raw()) are never filtered. Any setter below persists the
// change and resets the filter state (a brief re-fill transient).
void  current_set_filter_mode(int mode);   // CURRENT_FILTER_EMA / CURRENT_FILTER_BOXCAR
void  current_set_ema_alpha(float alpha);  // clamped to (0, 1]
void  current_set_boxcar_n(int n);         // clamped to [1, CURRENT_BOXCAR_MAX_N]
int   current_get_filter_mode(void);
float current_get_ema_alpha(void);
int   current_get_boxcar_n(void);

#ifdef __cplusplus
}
#endif

#endif // HEX_LEG_CURRENT_H
