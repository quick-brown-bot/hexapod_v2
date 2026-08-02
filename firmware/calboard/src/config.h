// Compile-time configuration for the current-sensor calibration adapter
// board (bench tool only — not a robot board). See
// hardware/calboard/calboard_sch.py for the schematic this pin map must
// match, and firmware/leg/README.md / tools/current_calibration/ for how
// this board is used.

#ifndef HEX_CALBOARD_CONFIG_H
#define HEX_CALBOARD_CONFIG_H

#include <stdint.h>

// --- Channels --------------------------------------------------------------
#define NUM_CHANNELS 3
enum { CH_COXA = 0, CH_FEMUR = 1, CH_TIBIA = 2 };

// --- RP2040 GPIO assignments (XIAO RP2040) ---------------------------------
// PWM decode inputs — wired to the LegBoard's servo signal pin (J2/J3/J4
// pin 1) instead of a real servo.
#define PIN_PWMIN_COXA   0  // GP0 (XIAO D6)
#define PIN_PWMIN_FEMUR  1  // GP1 (XIAO D7)
#define PIN_PWMIN_TIBIA  2  // GP2 (XIAO D8)

// Rail-voltage readback (ADC). RP2040 ADC inputs: GP26=ADC0 .. GP28=ADC2.
// GP29/ADC3 is left spare.
#define PIN_VSENSE_COXA   26  // ADC0
#define PIN_VSENSE_FEMUR  27  // ADC1
#define PIN_VSENSE_TIBIA  28  // ADC2
#define ADC_CH_COXA   0
#define ADC_CH_FEMUR  1
#define ADC_CH_TIBIA  2

// Two daisy-chained 74HC595 shift registers drive all 3*4=12 MOSFET gates
// from 3 GPIO. Bit order: channel 0 (coxa) bits 0-3, channel 1 (femur) bits
// 4-7, channel 2 (tibia) bits 8-11 — see ladder.cpp.
#define PIN_SR_SER    4  // GP4 (XIAO D9)  -> 74HC595 SER (serial data)
#define PIN_SR_SRCLK  3  // GP3 (XIAO D10) -> 74HC595 SRCLK (shift clock)
#define PIN_SR_RCLK   6  // GP6 (XIAO D4)  -> 74HC595 RCLK (latch)

// --- Servo PWM decode -------------------------------------------------------
// Matches DEFAULT_PWM_MIN_US/DEFAULT_PWM_MAX_US in firmware/leg/src/config.h
// — the LegBoard's default (uncalibrated) servo pulse range.
#define PWM_DECODE_MIN_US   1000
#define PWM_DECODE_MAX_US   2000
#define LADDER_STEPS        15  // non-zero steps per channel (4-bit ladder)

// --- Resistor ladder (per channel) ------------------------------------------
// Binary-weighted resistor legs, each switched to GND by its own MOSFET.
// Effective resistance for a bit-code 0-15 is the parallel combination of
// active legs (see ladder.cpp); with these values a bit's conductance
// roughly doubles per step, so current scales close to linearly with code.
//
// PLACEHOLDER VALUES — reviewed for a modest bench-test current range
// (~27 mA to ~490 mA at 6V) to keep worst-case single-leg dissipation
// low(ish); R4 alone still dissipates ~1.6W at 6V and needs a real power
// resistor (TO-220 / 2512 chip, not 0805). MUST be re-checked against the
// actual servos' expected current draw and resistor power ratings before
// this board is fabricated — see hardware/calboard/calboard_sch.py.
#define LADDER_R1_OHM  220.0f  // bit 0 (weakest)
#define LADDER_R2_OHM  100.0f  // bit 1
#define LADDER_R3_OHM   47.0f  // bit 2
#define LADDER_R4_OHM   22.0f  // bit 3 (strongest)

// --- ADC / voltage divider ---------------------------------------------------
#define ADC_RESOLUTION_BITS  12
#define ADC_MAX_COUNT        ((1 << ADC_RESOLUTION_BITS) - 1)
#define ADC_VREF_MV          3300
// Divider from the (up to ~6V) branch rail down into the RP2040's 3.3V ADC
// range: VSENSE_DIVIDER_RATIO = R_bottom / (R_top + R_bottom). Tune once the
// schematic's actual divider resistors are chosen.
#define VSENSE_DIVIDER_RATIO  0.5f

#endif // HEX_CALBOARD_CONFIG_H
