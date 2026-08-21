#include "servo.h"
#include "persist.h"

#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"

static const int s_pin[NUM_JOINTS] = { PIN_PWM_COXA, PIN_PWM_FEMUR, PIN_PWM_TIBIA };
static servo_calib_t s_cal[NUM_JOINTS];
static bool s_override[NUM_JOINTS];

static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static void configure_slice_for_pin(int pin)
{
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(pin);
    // 1 tick == 1 us: divide the system clock down to 1 MHz.
    float div = (float)clock_get_hz(clk_sys) / 1000000.0f;
    pwm_set_clkdiv(slice, div);
    pwm_set_wrap(slice, SERVO_PERIOD_US - 1); // 20000 us period -> 50 Hz
    pwm_set_enabled(slice, true);
}

void servo_init(void)
{
    for (int j = 0; j < NUM_JOINTS; ++j) {
        s_cal[j].angle_min_deg  = DEFAULT_ANGLE_MIN_DEG;
        s_cal[j].angle_max_deg  = DEFAULT_ANGLE_MAX_DEG;
        s_cal[j].pwm_min_us     = DEFAULT_PWM_MIN_US;
        s_cal[j].pwm_neutral_us = persist_get_pwm_neutral_us(j);
        s_cal[j].pwm_max_us     = DEFAULT_PWM_MAX_US;
        s_cal[j].invert         = persist_get_invert(j);
        s_cal[j].offset_deg     = 0.0f;
        s_override[j] = false;
        configure_slice_for_pin(s_pin[j]);
        // Start at this joint's calibrated neutral.
        pwm_set_gpio_level(s_pin[j], (uint16_t)s_cal[j].pwm_neutral_us);
    }
}

servo_calib_t *servo_get_calib(int joint)
{
    if (joint < 0 || joint >= NUM_JOINTS) return 0;
    return &s_cal[joint];
}

void servo_reload_pwm_neutral(int joint)
{
    if (joint < 0 || joint >= NUM_JOINTS) return;
    s_cal[joint].pwm_neutral_us = persist_get_pwm_neutral_us(joint);
}

void servo_reload_invert(int joint)
{
    if (joint < 0 || joint >= NUM_JOINTS) return;
    s_cal[joint].invert = persist_get_invert(joint);
}

bool servo_write_angle(int joint, float angle_deg)
{
    if (joint < 0 || joint >= NUM_JOINTS) return false;
    if (s_override[joint]) return false; // raw-pulse override active; see servo_write_pulse_us()
    const servo_calib_t *c = &s_cal[joint];

    float a = (float)c->invert * (angle_deg + c->offset_deg);
    float clamped = clampf(a, c->angle_min_deg, c->angle_max_deg);
    bool was_clamped = (clamped != a);

    // Two-segment mapping anchored at pwm_neutral_us (angle 0), rather than a
    // single line across [angle_min,angle_max]->[pwm_min,pwm_max] -- lets a
    // leg's true physical center be recalibrated (PWMNEUTRAL) independently
    // of the endpoint pulses. Degenerate ranges that don't straddle 0 just
    // fall back to the neutral pulse for that side.
    int32_t pulse;
    if (clamped >= 0.0f) {
        float span = c->angle_max_deg;
        float t = (span > 0.0f) ? (clamped / span) : 0.0f;
        pulse = c->pwm_neutral_us + (int32_t)(t * (float)(c->pwm_max_us - c->pwm_neutral_us));
    } else {
        float span = -c->angle_min_deg;
        float t = (span > 0.0f) ? (-clamped / span) : 0.0f;
        pulse = c->pwm_neutral_us - (int32_t)(t * (float)(c->pwm_neutral_us - c->pwm_min_us));
    }

    if (pulse < c->pwm_min_us) pulse = c->pwm_min_us;
    if (pulse > c->pwm_max_us) pulse = c->pwm_max_us;

    pwm_set_gpio_level(s_pin[joint], (uint16_t)pulse);
    return was_clamped;
}

bool servo_write_pulse_us(int joint, int32_t pulse_us)
{
    if (joint < 0 || joint >= NUM_JOINTS) return false;
    const servo_calib_t *c = &s_cal[joint];

    int32_t clamped = pulse_us;
    if (clamped < c->pwm_min_us) clamped = c->pwm_min_us;
    if (clamped > c->pwm_max_us) clamped = c->pwm_max_us;

    s_override[joint] = true;
    pwm_set_gpio_level(s_pin[joint], (uint16_t)clamped);
    return clamped != pulse_us;
}

void servo_clear_override(int joint)
{
    if (joint < 0 || joint >= NUM_JOINTS) return;
    s_override[joint] = false;
}
