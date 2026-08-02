#include "usbserial.h"
#include "config.h"
#include "ladder.h"
#include "pwm_capture.h"
#include "adc_readback.h"

#include <Arduino.h>
#include <string.h>
#include <stdlib.h>

#define LINE_MAX 64
#define FW_IDENT "hexapod-calboard fw v1"

static char   s_line[LINE_MAX];
static size_t s_len = 0;
static bool   s_manual[NUM_CHANNELS] = { false, false, false };

static const char *s_channel_name[NUM_CHANNELS] = { "coxa", "femur", "tibia" };

static void print_help(void)
{
    Serial.println(F("commands:"));
    Serial.println(F("  PING              -> PONG <fw>"));
    Serial.println(F("  STATUS?           -> identity"));
    Serial.println(F("  READ?             -> per-channel step/R/V/I, all 3 channels"));
    Serial.println(F("  STEP <ch 0-2> <n 0-15> -> manual load override (bring-up)"));
    Serial.println(F("  AUTO <ch 0-2>     -> resume PWM-decode-driven control"));
    Serial.println(F("  HELP"));
}

// Applies PWM-decoded steps to any channel not under manual override. Called
// every poll so the adapter tracks the LegBoard's servo output continuously.
static void apply_auto_steps(void)
{
    for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
        if (s_manual[ch]) continue;

        if (!pwm_capture_is_fresh(ch)) {
            ladder_set_step(ch, 0); // source silent: no load
            continue;
        }

        int32_t us = pwm_capture_get_us(ch);
        if (us < PWM_DECODE_MIN_US) us = PWM_DECODE_MIN_US;
        if (us > PWM_DECODE_MAX_US) us = PWM_DECODE_MAX_US;

        float t = (float)(us - PWM_DECODE_MIN_US) / (float)(PWM_DECODE_MAX_US - PWM_DECODE_MIN_US);
        int step = (int)(t * (float)LADDER_STEPS + 0.5f);
        ladder_set_step(ch, (uint8_t)step);
    }
}

static void handle_read(void)
{
    for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
        uint8_t step = ladder_get_step(ch);
        float r_eff = ladder_effective_ohms(step);
        float v = adc_readback_get_volts(ch);
        float i_ma = (step == 0) ? 0.0f : (v / r_eff) * 1000.0f;

        Serial.print(F("CH"));
        Serial.print(ch);
        Serial.print(F(" "));
        Serial.print(s_channel_name[ch]);
        Serial.print(F(" step="));
        Serial.print(step);
        Serial.print(F(" R_ohm="));
        Serial.print(r_eff, 2);
        Serial.print(F(" V="));
        Serial.print(v, 3);
        Serial.print(F(" I_mA="));
        Serial.print(i_ma, 1);
        Serial.print(F(" mode="));
        Serial.println(s_manual[ch] ? F("manual") : F("auto"));
    }
}

// Parses "STEP <ch> <n>"; args points just past "STEP ".
static void handle_step(char *args)
{
    char *end;
    long ch = strtol(args, &end, 10);
    if (end == args) { Serial.println(F("ERR usage: STEP <ch 0-2> <n 0-15>")); return; }
    long n = strtol(end, &end, 10);
    if (end == args || ch < 0 || ch >= NUM_CHANNELS) {
        Serial.println(F("ERR usage: STEP <ch 0-2> <n 0-15>"));
        return;
    }

    s_manual[ch] = true;
    ladder_set_step((int)ch, (uint8_t)n);
    Serial.print(F("OK STEP CH"));
    Serial.print(ch);
    Serial.print(F("="));
    Serial.println(ladder_get_step((int)ch));
}

// Parses "AUTO <ch>"; args points just past "AUTO ".
static void handle_auto(char *args)
{
    char *end;
    long ch = strtol(args, &end, 10);
    if (end == args || ch < 0 || ch >= NUM_CHANNELS) {
        Serial.println(F("ERR usage: AUTO <ch 0-2>"));
        return;
    }
    s_manual[ch] = false;
    Serial.print(F("OK AUTO CH"));
    Serial.println(ch);
}

static void handle_line(char *line)
{
    while (*line == ' ') ++line;

    if (strcmp(line, "PING") == 0) {
        Serial.println(F("PONG " FW_IDENT));
    } else if (strcmp(line, "HELP") == 0) {
        print_help();
    } else if (strcmp(line, "STATUS?") == 0) {
        Serial.println(F("fw=" FW_IDENT));
    } else if (strcmp(line, "READ?") == 0) {
        handle_read();
    } else if (strncmp(line, "STEP ", 5) == 0) {
        handle_step(line + 5);
    } else if (strncmp(line, "AUTO ", 5) == 0) {
        handle_auto(line + 5);
    } else if (line[0] != '\0') {
        Serial.println(F("ERR unknown command (try HELP)"));
    }
}

void usbserial_init(void)
{
    Serial.begin(115200);
    s_len = 0;
}

void usbserial_poll(void)
{
    apply_auto_steps();

    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (s_len > 0) {
                s_line[s_len] = '\0';
                handle_line(s_line);
                s_len = 0;
            }
        } else if (s_len + 1 < LINE_MAX) {
            s_line[s_len++] = c;
        } else {
            s_len = 0; // overflow: resync
        }
    }
}
