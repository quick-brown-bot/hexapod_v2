#include "servo.h"

#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"

static const int s_pin[NUM_JOINTS] = { PIN_PWM_COXA, PIN_PWM_FEMUR, PIN_PWM_TIBIA };
static servo_calib_t s_cal[NUM_JOINTS];

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
        s_cal[j].pwm_neutral_us = DEFAULT_PWM_NEUTRAL_US;
        s_cal[j].pwm_max_us     = DEFAULT_PWM_MAX_US;
        s_cal[j].invert         = 1;
        s_cal[j].offset_deg     = 0.0f;
        configure_slice_for_pin(s_pin[j]);
        // Start at neutral.
        pwm_set_gpio_level(s_pin[j], (uint16_t)s_cal[j].pwm_neutral_us);
    }
}

servo_calib_t *servo_get_calib(int joint)
{
    if (joint < 0 || joint >= NUM_JOINTS) return 0;
    return &s_cal[joint];
}

bool servo_write_angle(int joint, float angle_deg)
{
    if (joint < 0 || joint >= NUM_JOINTS) return false;
    const servo_calib_t *c = &s_cal[joint];

    float a = (float)c->invert * (angle_deg + c->offset_deg);
    float clamped = clampf(a, c->angle_min_deg, c->angle_max_deg);
    bool was_clamped = (clamped != a);

    float span = c->angle_max_deg - c->angle_min_deg;
    float t = (span > 0.0f) ? (clamped - c->angle_min_deg) / span : 0.5f;
    int32_t pulse = c->pwm_min_us + (int32_t)(t * (float)(c->pwm_max_us - c->pwm_min_us));

    if (pulse < c->pwm_min_us) pulse = c->pwm_min_us;
    if (pulse > c->pwm_max_us) pulse = c->pwm_max_us;

    pwm_set_gpio_level(s_pin[joint], (uint16_t)pulse);
    return was_clamped;
}
