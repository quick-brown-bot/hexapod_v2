#include "calib.h"
#include "persist.h"
#include "config.h"
#include "servo.h"
#include "current.h"
#include "loopstat.h"

#include <Arduino.h>
#include <string.h>
#include <stdlib.h>

#define CALIB_LINE_MAX 64
#define FW_IDENT "hexapod-leg fw v1"

static char   s_line[CALIB_LINE_MAX];
static size_t s_len = 0;

static const char *s_channel_name[NUM_CURRENT_CHANNELS] = { "total", "coxa", "femur", "tibia" };

static void print_help(void)
{
    Serial.println(F("commands:"));
    Serial.println(F("  PING        -> PONG <fw>"));
    Serial.println(F("  ADDR?       -> ADDR=<n>"));
    Serial.println(F("  ADDR <1-6>  -> set & persist leg address"));
    Serial.println(F("  STATUS?     -> address + identity"));
    Serial.println(F("  PWM <joint 0-2> <us>            -> raw pulse override (bring-up/calib)"));
    Serial.println(F("  CURRAW?                          -> raw ADC counts+mV, all 4 channels"));
    Serial.println(F("  CURCAL?                          -> current scale=/offset=, all 4 channels"));
    Serial.println(F("  CURCAL <ch 0-3> <scale> <offset> -> set & persist channel current calibration"));
    Serial.println(F("  CURFILT?                         -> current smoothing mode + params (persisted)"));
    Serial.println(F("  CURFILT EMA [alpha 0-1]          -> switch to/tune EMA smoothing (default)"));
    Serial.println(F("  CURFILT BOXCAR <n 1-32>          -> switch to N-sample moving average"));
    Serial.println(F("  LOOPSTAT?                        -> measured loop()/control-step Hz (1s window)"));
    Serial.println(F("  HELP"));
}

static void handle_loopstat(void)
{
    Serial.print(F("loop_hz="));
    Serial.print(loopstat_get_loop_hz());
    Serial.print(F(" control_hz="));
    Serial.println(loopstat_get_control_hz());
}

static void handle_curraw(void)
{
    current_raw_t raw;
    current_get_raw(&raw);
    for (int ch = 0; ch < NUM_CURRENT_CHANNELS; ++ch) {
        Serial.print(F("CH"));
        Serial.print(ch);
        Serial.print(F(" "));
        Serial.print(s_channel_name[ch]);
        Serial.print(F(" counts="));
        Serial.print(raw.raw_counts[ch]);
        Serial.print(F(" mv="));
        Serial.println(raw.raw_mv[ch], 2);
    }
}

static void handle_curcal_query(void)
{
    for (int ch = 0; ch < NUM_CURRENT_CHANNELS; ++ch) {
        current_calib_t c = persist_get_current_calib(ch);
        Serial.print(F("CH"));
        Serial.print(ch);
        Serial.print(F(" "));
        Serial.print(s_channel_name[ch]);
        Serial.print(F(" scale="));
        Serial.print(c.scale_ma_per_mv, 6);
        Serial.print(F(" offset="));
        Serial.println(c.offset_ma, 3);
    }
}

// Parses "CURCAL <ch> <scale> <offset>"; args points just past "CURCAL ".
static void handle_curcal_set(char *args)
{
    char *end;
    long ch = strtol(args, &end, 10);
    if (end == args) { Serial.println(F("ERR usage: CURCAL <ch 0-3> <scale> <offset>")); return; }
    float scale = strtod(end, &end);
    float offset = strtod(end, &end);

    if (persist_set_current_calib((int)ch, scale, offset)) {
        current_reload_calib();
        Serial.print(F("OK CH"));
        Serial.print(ch);
        Serial.print(F(" scale="));
        Serial.print(scale, 6);
        Serial.print(F(" offset="));
        Serial.println(offset, 3);
    } else {
        Serial.println(F("ERR channel out of range (0-3)"));
    }
}

static void handle_curfilt_query(void)
{
    if (current_get_filter_mode() == CURRENT_FILTER_BOXCAR) {
        Serial.print(F("CURFILT=BOXCAR N="));
        Serial.println(current_get_boxcar_n());
    } else {
        Serial.print(F("CURFILT=EMA ALPHA="));
        Serial.println(current_get_ema_alpha(), 4);
    }
}

// Parses "EMA [alpha]" or "BOXCAR <n>"; args points just past "CURFILT ".
static void handle_curfilt_set(char *args)
{
    if (strncmp(args, "EMA", 3) == 0) {
        current_set_filter_mode(CURRENT_FILTER_EMA);
        char *rest = args + 3;
        while (*rest == ' ') ++rest;
        if (*rest != '\0') {
            char *end;
            float alpha = strtod(rest, &end);
            if (end != rest) current_set_ema_alpha(alpha);
        }
        Serial.print(F("OK CURFILT=EMA ALPHA="));
        Serial.println(current_get_ema_alpha(), 4);
        return;
    }
    if (strncmp(args, "BOXCAR", 6) == 0) {
        char *end;
        long n = strtol(args + 6, &end, 10);
        if (end == args + 6) {
            Serial.println(F("ERR usage: CURFILT BOXCAR <n 1-32>"));
            return;
        }
        current_set_filter_mode(CURRENT_FILTER_BOXCAR);
        current_set_boxcar_n((int)n);
        Serial.print(F("OK CURFILT=BOXCAR N="));
        Serial.println(current_get_boxcar_n());
        return;
    }
    Serial.println(F("ERR usage: CURFILT EMA [alpha 0-1] | CURFILT BOXCAR <n 1-32>"));
}

// Parses "PWM <joint> <us>"; args points just past "PWM ".
static void handle_pwm(char *args)
{
    char *end;
    long joint = strtol(args, &end, 10);
    if (end == args) { Serial.println(F("ERR usage: PWM <joint 0-2> <us>")); return; }
    long us = strtol(end, &end, 10);
    if (end == args) { Serial.println(F("ERR usage: PWM <joint 0-2> <us>")); return; }

    bool clamped = servo_write_pulse_us((int)joint, (int32_t)us);
    if (joint < 0 || joint >= NUM_JOINTS) {
        Serial.println(F("ERR joint out of range (0-2)"));
        return;
    }
    Serial.print(F("OK PWM joint="));
    Serial.print(joint);
    Serial.print(F(" us="));
    Serial.print(us);
    Serial.println(clamped ? F(" (clamped)") : F(""));
}

static void handle_line(char *line)
{
    // Trim leading spaces.
    while (*line == ' ') ++line;

    if (strcmp(line, "PING") == 0) {
        Serial.println(F("PONG " FW_IDENT));
    } else if (strcmp(line, "HELP") == 0) {
        print_help();
    } else if (strcmp(line, "ADDR?") == 0) {
        Serial.print(F("ADDR="));
        Serial.println(persist_get_address());
    } else if (strncmp(line, "ADDR ", 5) == 0) {
        int a = atoi(line + 5);
        if (persist_set_address((uint8_t)a)) {
            Serial.print(F("OK ADDR="));
            Serial.println(persist_get_address());
        } else {
            Serial.println(F("ERR address out of range (1-6)"));
        }
    } else if (strcmp(line, "STATUS?") == 0) {
        Serial.print(F("fw=" FW_IDENT " addr="));
        Serial.println(persist_get_address());
    } else if (strcmp(line, "CURRAW?") == 0) {
        handle_curraw();
    } else if (strcmp(line, "CURCAL?") == 0) {
        handle_curcal_query();
    } else if (strncmp(line, "CURCAL ", 7) == 0) {
        handle_curcal_set(line + 7);
    } else if (strcmp(line, "CURFILT?") == 0) {
        handle_curfilt_query();
    } else if (strncmp(line, "CURFILT ", 8) == 0) {
        handle_curfilt_set(line + 8);
    } else if (strcmp(line, "LOOPSTAT?") == 0) {
        handle_loopstat();
    } else if (strncmp(line, "PWM ", 4) == 0) {
        handle_pwm(line + 4);
    } else if (line[0] != '\0') {
        Serial.println(F("ERR unknown command (try HELP)"));
    }
}

void calib_init(void)
{
    Serial.begin(115200);
    s_len = 0;
}

void calib_poll(void)
{
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (s_len > 0) {
                s_line[s_len] = '\0';
                handle_line(s_line);
                s_len = 0;
            }
        } else if (s_len + 1 < CALIB_LINE_MAX) {
            s_line[s_len++] = c;
        } else {
            s_len = 0; // overflow: resync
        }
    }
}
