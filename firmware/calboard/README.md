# Calibration Adapter Board Firmware (RP2040)

Bench-only firmware for the current-sensor calibration adapter board
(`hardware/calboard/`). This board is **not part of the robot** — it plugs
into a single LegBoard's servo connectors (J2/J3/J4) in place of real
servos, so the LegBoard's INA4181 current sensing can be calibrated against
a known load instead of a multimeter and manual math each time.

See:
- [`hardware/calboard/calboard_sch.py`](../../hardware/calboard/calboard_sch.py) — schematic (pin map source of truth)
- [`tools/current_calibration/`](../../tools/current_calibration/) — host script that drives this board + a LegBoard together
- [`firmware/leg/src/calib.cpp`](../leg/src/calib.cpp) — LegBoard-side `PWM`/`CURRAW?`/`CURCAL` commands this board's PWM decode pairs with

## How it works

Each of the 3 channels (coxa/femur/tibia) has its own 3-pin header matching
a LegBoard servo connector's pinout (signal / +6V branch / GND). The signal
pin feeds a GPIO edge-interrupt PWM decoder (`pwm_capture.*`); the decoded
pulse width (1000-2000us) maps to a 0-15 step, which selects a combination
of 4 binary-weighted resistor legs via two daisy-chained 74HC595 shift
registers (`ladder.*`). A resistor divider on the branch rail feeds back into
the RP2040 ADC (`adc_readback.*`) so the true load current is computed as
`I = V_measured / R_effective(step)` — accounting for rail sag under load —
rather than assumed from a fixed 6V.

## Toolchain

Same as `firmware/leg/` — PlatformIO with the arduino-pico core:

```bash
pio run                       # build
pio run --target upload       # flash
pio device monitor            # USB serial command interface
```

## Module Layout

```
src/
  config.h          Pin map, PWM decode range, ladder resistor values (placeholder — see below)
  pwm_capture.h/.cpp GPIO-interrupt PWM pulse-width capture, one channel per servo connector
  ladder.h/.cpp     Shift-register bit-banging + effective-resistance lookup table
  adc_readback.h/.cpp Branch rail voltage readback
  usbserial.h/.cpp  Minimal ASCII command interface (mirrors firmware/leg/src/calib.cpp's style)
  main.cpp          Wiring
```

## USB Commands

```
PING                     -> PONG <fw>
STATUS?                  -> identity
READ?                    -> per-channel: step, R_ohm, V, I_mA, mode (auto/manual)
STEP <ch 0-2> <n 0-15>   -> manual load override (bring-up/standalone testing)
AUTO <ch 0-2>            -> resume PWM-decode-driven control
HELP
```

Each channel defaults to `auto` mode (load selected by the incoming PWM
signal). `STEP` is for bringing up/testing this board on its own before
wiring it to a LegBoard.

## Bring-Up Status / TODO

Builds clean; not yet validated on hardware — no physical board has been
fabricated yet.

- **Resistor ladder values in `config.h` are placeholders** (`LADDER_R1..R4_OHM`),
  sized for a modest ~27-490 mA bench-test current range at 6V. Review
  against the actual servos' expected current draw, and check resistor
  power ratings before fab — `LADDER_R4_OHM` alone can dissipate ~1.6W at
  6V and needs a real power resistor (TO-220 / 2512 chip), not a small
  0805/1206 part.
- `VSENSE_DIVIDER_RATIO` in `config.h` is a placeholder; set it from the
  schematic's actual divider resistor values once chosen.
- Pin map must be verified against `hardware/calboard/calboard_sch.py`
  before wiring.
