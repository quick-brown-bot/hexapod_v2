#include "ladder.h"

#include <Arduino.h>

static uint16_t s_bits = 0;       // 4 bits per channel: [tibia|femur|coxa]
static uint8_t  s_step[NUM_CHANNELS] = {0, 0, 0};
static float    s_reff_table[LADDER_STEPS + 1];

static void shift_out_all(void)
{
    for (int i = 15; i >= 0; --i) {
        digitalWrite(PIN_SR_SER, (s_bits >> i) & 1);
        digitalWrite(PIN_SR_SRCLK, HIGH);
        digitalWrite(PIN_SR_SRCLK, LOW);
    }
    digitalWrite(PIN_SR_RCLK, HIGH);
    digitalWrite(PIN_SR_RCLK, LOW);
}

static void build_reff_table(void)
{
    const float r_leg[4] = { LADDER_R1_OHM, LADDER_R2_OHM, LADDER_R3_OHM, LADDER_R4_OHM };
    s_reff_table[0] = 1.0e9f; // no legs active: effectively open
    for (int step = 1; step <= LADDER_STEPS; ++step) {
        float conductance = 0.0f;
        for (int bit = 0; bit < 4; ++bit) {
            if (step & (1 << bit)) conductance += 1.0f / r_leg[bit];
        }
        s_reff_table[step] = 1.0f / conductance;
    }
}

void ladder_init(void)
{
    pinMode(PIN_SR_SER, OUTPUT);
    pinMode(PIN_SR_SRCLK, OUTPUT);
    pinMode(PIN_SR_RCLK, OUTPUT);
    digitalWrite(PIN_SR_SER, LOW);
    digitalWrite(PIN_SR_SRCLK, LOW);
    digitalWrite(PIN_SR_RCLK, LOW);

    build_reff_table();

    s_bits = 0;
    shift_out_all(); // all legs off at boot
}

void ladder_set_step(int channel, uint8_t step)
{
    if (channel < 0 || channel >= NUM_CHANNELS) return;
    if (step > LADDER_STEPS) step = LADDER_STEPS;

    s_step[channel] = step;
    uint16_t mask = (uint16_t)0xF << (channel * 4);
    s_bits = (uint16_t)((s_bits & ~mask) | ((uint16_t)step << (channel * 4)));
    shift_out_all();
}

uint8_t ladder_get_step(int channel)
{
    if (channel < 0 || channel >= NUM_CHANNELS) return 0;
    return s_step[channel];
}

float ladder_effective_ohms(uint8_t step)
{
    if (step > LADDER_STEPS) step = LADDER_STEPS;
    return s_reff_table[step];
}
