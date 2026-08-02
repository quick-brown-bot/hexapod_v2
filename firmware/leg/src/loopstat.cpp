#include "loopstat.h"

#include <Arduino.h>

#define STAT_WINDOW_US 1000000UL

static uint32_t s_window_start_us;
static uint32_t s_loop_count;
static uint32_t s_control_count;
static uint32_t s_last_loop_hz;
static uint32_t s_last_control_hz;

void loopstat_init(void)
{
    s_window_start_us = micros();
    s_loop_count = 0;
    s_control_count = 0;
    s_last_loop_hz = 0;
    s_last_control_hz = 0;
}

static void maybe_roll_window(uint32_t now_us)
{
    if ((uint32_t)(now_us - s_window_start_us) >= STAT_WINDOW_US) {
        s_last_loop_hz = s_loop_count;
        s_last_control_hz = s_control_count;
        s_loop_count = 0;
        s_control_count = 0;
        s_window_start_us = now_us;
    }
}

void loopstat_tick_loop(void)
{
    uint32_t now = micros();
    s_loop_count++;
    maybe_roll_window(now);
}

void loopstat_tick_control(void)
{
    s_control_count++;
}

uint32_t loopstat_get_loop_hz(void)    { return s_last_loop_hz; }
uint32_t loopstat_get_control_hz(void) { return s_last_control_hz; }
