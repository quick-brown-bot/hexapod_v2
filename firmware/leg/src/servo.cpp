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

    // Uniform rate across the whole calibrated pulse range, anchored at
    // pwm_neutral_us (angle 0). pwm_neutral_us shifts *where zero degrees
    // lands* -- correcting this leg's mechanical/mounting offset -- it does
    // not change the slope. The previous two-segment mapping instead
    // recomputed a separate rate on each side of neutral by dividing by a
    // fixed 90 deg, which silently assumed the servo's real response is
    // asymmetric around whatever pulse its calibrated center happens to
    // land on -- wrong for a typical analog servo, whose pulse-to-angle
    // response is uniform across its full rated range regardless of where
    // manufacturing tolerance puts that center. A consequence: with an
    // off-center neutral, the reachable range is now honestly slightly
    // asymmetric (clamped below) rather than silently compressed on the
    // tight side to fake a full angle_min..angle_max sweep on both sides.
    float total_span_deg = c->angle_max_deg - c->angle_min_deg; // typically 180
    float us_per_deg = (total_span_deg > 0.0f)
        ? (float)(c->pwm_max_us - c->pwm_min_us) / total_span_deg
        : 0.0f;
    int32_t pulse = c->pwm_neutral_us + (int32_t)(clamped * us_per_deg);

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
