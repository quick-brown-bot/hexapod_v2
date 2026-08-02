// Servo PWM driver — RP2040 hardware PWM at 50 Hz, one channel per joint.
//
// Calibration (angle->pulse) lives here on the leg; the ESP32 sends only the
// kinematic joint angle in degrees. See HARDWARE_AND_MECHANICS.md.

#ifndef HEX_LEG_SERVO_H
#define HEX_LEG_SERVO_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

// Per-servo calibration. Shaped for later flash persistence.
typedef struct {
    float    angle_min_deg;
    float    angle_max_deg;
    int32_t  pwm_min_us;
    int32_t  pwm_neutral_us;
    int32_t  pwm_max_us;
    int8_t   invert;        // +1 or -1
    float    offset_deg;    // added to commanded angle before mapping
} servo_calib_t;

// Initialize PWM hardware for all three joints with default calibration.
void servo_init(void);

// Get a pointer to the calibration for a joint (for the calibration interface).
servo_calib_t *servo_get_calib(int joint);

// Command a joint to an angle in degrees. Maps through calibration to a pulse
// width and updates the PWM channel. Returns true if the angle was clamped to
// the servo's physical range.
bool servo_write_angle(int joint, float angle_deg);

// Command a joint directly to a raw pulse width in microseconds, bypassing
// angle calibration. For bring-up/calibration use only (see calib.cpp's
// PWM command and tools/current_calibration) — clamps to
// [pwm_min_us, pwm_max_us] for that joint. Returns true if clamped.
//
// While a joint is in this raw-override state, servo_write_angle() for that
// joint is a no-op (so the 1 kHz interpolation control loop doesn't stomp
// the raw pulse every tick). The override is cleared automatically the next
// time a real RS485 pull frame sets a target for that joint — see
// servo_clear_override() — so normal operation resumes as soon as the
// master reconnects.
bool servo_write_pulse_us(int joint, int32_t pulse_us);

// Clear the raw-pulse override for a joint, restoring normal
// servo_write_angle() behavior. Called by main.cpp when a real RS485 pull
// sets a target for that joint.
void servo_clear_override(int joint);

#ifdef __cplusplus
}
#endif

#endif // HEX_LEG_SERVO_H
