#include "pwm_capture.h"

#include <Arduino.h>

#define PULSE_STALE_US  50000UL  // 50 ms — 2.5x the nominal 20 ms servo period

static const uint8_t s_pin[NUM_CHANNELS] = { PIN_PWMIN_COXA, PIN_PWMIN_FEMUR, PIN_PWMIN_TIBIA };

static volatile uint32_t s_rise_us[NUM_CHANNELS];
static volatile int32_t  s_pulse_us[NUM_CHANNELS];
static volatile uint32_t s_last_capture_us[NUM_CHANNELS];

// One ISR per channel (rather than a shared dispatcher) keeps each capture
// path simple and avoids needing to read which-pin-changed state.
template <int CH>
static void isr_edge(void)
{
    uint32_t now = micros();
    if (digitalRead(s_pin[CH]) == HIGH) {
        s_rise_us[CH] = now;
    } else {
        s_pulse_us[CH] = (int32_t)(now - s_rise_us[CH]);
        s_last_capture_us[CH] = now;
    }
}

void pwm_capture_init(void)
{
    for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
        s_rise_us[ch] = 0;
        s_pulse_us[ch] = 0;
        s_last_capture_us[ch] = 0;
        pinMode(s_pin[ch], INPUT);
    }
    attachInterrupt(digitalPinToInterrupt(PIN_PWMIN_COXA),  isr_edge<CH_COXA>,  CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_PWMIN_FEMUR), isr_edge<CH_FEMUR>, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_PWMIN_TIBIA), isr_edge<CH_TIBIA>, CHANGE);
}

int32_t pwm_capture_get_us(int channel)
{
    if (channel < 0 || channel >= NUM_CHANNELS) return 0;
    noInterrupts();
    int32_t v = s_pulse_us[channel];
    interrupts();
    return v;
}

bool pwm_capture_is_fresh(int channel)
{
    if (channel < 0 || channel >= NUM_CHANNELS) return false;
    noInterrupts();
    uint32_t last = s_last_capture_us[channel];
    interrupts();
    return (uint32_t)(micros() - last) < PULSE_STALE_US;
}
