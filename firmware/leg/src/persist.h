// Flash-backed persistence for the per-leg identity and current-sensor
// calibration, via arduino-pico's EEPROM.h (RP2040 flash-sector emulation).
//
// Two independent partitions within that EEPROM address space:
//   - identity: the leg address (1..6), or 0 if never calibrated. Must
//     differ between otherwise-identical boards and survive reboot. Written
//     once during pre-mount calibration over USB (ADDR <1-6>).
//   - calib: per-channel current-sense scale/offset, plus the current
//     smoothing strategy (CURFILT, filter mode + boxcar N). Board-specific,
//     set once via the calibration bench (tools/current_calibration) or
//     tools/leg_configurator.py over the leg's own USB serial.
// Each partition has its own magic/version so resetting one does not affect
// the other. Runtime parameters (MOVE_DURATION, etc.) are NOT persisted —
// they default in firmware and the ESP32 re-applies them on recovery (see
// RS485_PROTOCOL.md "simplest first").

#ifndef HEX_LEG_PERSIST_H
#define HEX_LEG_PERSIST_H

#include "config.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float scale_ma_per_mv;
    float offset_ma;
} current_calib_t;

// Load persisted state from flash; falls back to defaults if absent/invalid.
void persist_init(void);

// 0 means the board has never been calibrated (see DEFAULT_LEG_ADDR).
uint8_t persist_get_address(void);

// Set and persist the leg address. Returns false for an out-of-range address.
bool persist_set_address(uint8_t addr);

// Get the calibration for one current channel (0..NUM_CURRENT_CHANNELS-1,
// matching ADC_CH_TOTAL/COXA/FEMUR/TIBIA). Returns defaults for an
// out-of-range channel.
current_calib_t persist_get_current_calib(int channel);

// Set and persist the calibration for one current channel. Returns false for
// an out-of-range channel.
bool persist_set_current_calib(int channel, float scale_ma_per_mv, float offset_ma);

// Current-smoothing strategy (CURRENT_FILTER_EMA/CURRENT_FILTER_BOXCAR,
// config.h), EMA alpha, and boxcar window N. Persisted so CURFILT survives
// reboot.
int   persist_get_current_filter_mode(void);
float persist_get_current_ema_alpha(void);
int   persist_get_current_boxcar_n(void);
bool  persist_set_current_filter(int mode, float ema_alpha, int boxcar_n);

// Per-joint PWM neutral (center) pulse width in microseconds -- the pulse
// that a commanded angle of 0 degrees maps to (see servo.cpp). Lets a leg's
// true physical center be shifted away from DEFAULT_PWM_NEUTRAL_US to
// compensate for a servo that can't be mounted perfectly centered on this
// leg's mechanical build. Persisted per joint (JOINT_COXA/FEMUR/TIBIA), set
// over USB (calib.cpp PWMNEUTRAL / tools/leg_configurator.py --set-neutral).
int32_t persist_get_pwm_neutral_us(int joint);
bool    persist_set_pwm_neutral_us(int joint, int32_t pwm_neutral_us);

// Per-joint sign inversion (+1 or -1), applied to the commanded angle before
// the angle->pulse mapping (see servo.cpp servo_write_angle()). Reconciles
// this leg's physical mounting/wiring with the mainboard IK's leg-local sign
// convention (docs/architecture/HARDWARE_AND_MECHANICS.md "Coordinate
// Conventions", hex_kinematics/leg.c) -- there is no guarantee a given
// joint's raw "positive PWM direction" already matches what the IK model
// calls positive for that joint. Defaults to +1 (uninverted). Persisted per
// joint, set over USB (calib.cpp INVERT / tools/leg_configurator.py).
int8_t persist_get_invert(int joint);
bool   persist_set_invert(int joint, int8_t invert);

#ifdef __cplusplus
}
#endif

#endif // HEX_LEG_PERSIST_H
