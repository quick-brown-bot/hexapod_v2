// Current-sensor calibration adapter board firmware (RP2040 / arduino-pico).
//
// Bench tool only — plugs into a LegBoard's servo connectors (J2/J3/J4) in
// place of real servos. Decodes the incoming servo PWM signal per channel,
// switches a known binary-weighted resistor ladder to present a known load,
// and reports the ground-truth load (selected resistance, measured rail
// voltage, computed current) over USB serial so a host script can pair it
// against the LegBoard's own current-sensor reading.
//
// See:
//   hardware/calboard/calboard_sch.py  — schematic this pin map must match
//   tools/current_calibration/         — host-side calibration script
//   firmware/leg/src/calib.cpp         — LegBoard-side PWM/CURRAW/CURCAL commands

#include <Arduino.h>

#include "config.h"
#include "ladder.h"
#include "pwm_capture.h"
#include "adc_readback.h"
#include "usbserial.h"

void setup()
{
    usbserial_init();
    ladder_init();
    pwm_capture_init();
    adc_readback_init();
}

void loop()
{
    usbserial_poll(); // also applies PWM-decoded steps to the ladder
}
