#include "status_led.h"
#include "config.h"

#include <Arduino.h>

static bool     s_led_on = false;
static uint32_t s_last_toggle_us = 0;

static inline void led_write(bool on)
{
    bool level_high = STATUS_LED_ACTIVE_LOW ? !on : on;
    digitalWrite(PIN_STATUS_LED_R, level_high ? HIGH : LOW);
}

void status_led_init(void)
{
    pinMode(PIN_STATUS_LED_R, OUTPUT);
    s_led_on = false;
    s_last_toggle_us = 0;
    led_write(false);
}

void status_led_update(bool calibrated, uint32_t now_us)
{
    if (calibrated) {
        if (s_led_on) {
            s_led_on = false;
            led_write(false);
        }
        return;
    }

    if ((uint32_t)(now_us - s_last_toggle_us) >= STATUS_LED_BLINK_HALF_PERIOD_US) {
        s_last_toggle_us = now_us;
        s_led_on = !s_led_on;
        led_write(s_led_on);
    }
}
