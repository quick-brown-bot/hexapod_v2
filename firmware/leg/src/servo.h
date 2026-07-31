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

#ifdef __cplusplus
}
#endif

#endif // HEX_LEG_SERVO_H
