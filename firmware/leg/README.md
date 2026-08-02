# LegBoard Firmware (RP2040)

RP2040 firmware for a single hexapod leg. One binary runs on all six
LegBoards; each board differs only by its stored RS485 address.

The LegBoard is an RS485 slave: it receives joint-angle targets from the ESP32
MainBoard, interpolates between them locally for smooth motion, drives three
servos, samples per-servo current, and answers every pull with a telemetry
response. If the bus goes silent it holds position (watchdog).

See:
- [`docs/architecture/HARDWARE_AND_MECHANICS.md`](../../docs/architecture/HARDWARE_AND_MECHANICS.md) — pins, power, mechanics
- [`docs/interfaces/RS485_PROTOCOL.md`](../../docs/interfaces/RS485_PROTOCOL.md) — wire protocol
- [`docs/architecture/SYSTEM_ARCHITECTURE.md`](../../docs/architecture/SYSTEM_ARCHITECTURE.md) — system context
- [`docs/development/README.md`](../../docs/development/README.md) — toolchain & bring-up

## Toolchain

PlatformIO with the **arduino-pico** core (Earle Philhower). The official
PlatformIO `raspberrypi` platform ships only the mbed core and a couple of
boards, so `platformio.ini` uses the maxgerhardt platform fork plus
`board_build.core = earlephilhower`. This core sits directly on the Pico SDK,
so the firmware uses native `hardware/uart.h`, `hardware/pwm.h`, and
`hardware/adc.h` alongside Arduino APIs.

```bash
pio run                       # build
pio run --target upload       # flash (or drag the .uf2 via BOOTSEL)
pio device monitor            # USB serial (calibration interface)
```

## Module Layout

```
src/
  config.h       Pin map, loop rate, parameter and calibration defaults
  protocol.h/.c  RS485 frame parse/build + CRC-8/SMBus (host-testable, no HW deps)
  rs485.h/.cpp   UART0 half-duplex transport with manual DE turnaround
  interp.h/.c    Local LINEAR / cubic-Hermite interpolation between targets
  servo.h/.cpp   Hardware PWM at 50 Hz, angle->pulse via per-servo calibration
  current.h/.cpp INA4181 current sensing via the RP2040 ADC
  persist.h/.cpp Flash-backed leg address + current calibration (arduino-pico EEPROM,
                 two independent partitions)
  status_led.h/.cpp  Onboard LED: blinks red 2 Hz while uncalibrated, off once calibrated
  calib.h/.cpp   Minimal USB-serial bring-up interface (set leg address)
  main.cpp       Wiring: RS485 request/response + fixed-rate control loop
test/
  host/          Host unit test for the protocol module (run where gcc exists)
```

## Design Notes

- **Protocol parity.** `proto_crc8()` and the frame formats must stay
  byte-for-byte identical to the ESP32 master
  (`firmware/mainboard/components/hex_rs485_master`). The CRC examples in
  `RS485_PROTOCOL.md` are real CRC-8/SMBus values and double as test vectors.
- **DE turnaround.** arduino-pico has no hardware auto-DE. `rs485_send()` drives
  the DE pin manually and polls the UART BUSY flag before releasing it, so the
  final CRC byte is not truncated at 1 Mbps.
- **Silence is the protocol.** On CRC failure or an address mismatch the leg
  sends nothing; the master treats silence as a timeout. The leg never NAKs.
- **Persistence split.** Only the leg address and current-sense calibration are
  stored in flash, in two independent EEPROM.h partitions (`persist.cpp`) so
  resetting one doesn't touch the other. Runtime parameters (move duration,
  watchdog timeout, interpolation mode, joint limits) default in firmware and
  are re-applied by the ESP32 on recovery — matching the protocol doc's
  "simplest first" approach. Current-sense calibration is set locally over
  USB (`CURCAL`) only — it's a board-specific constant the leg owns, not
  something the ESP32 needs to know or re-apply, so it deliberately has no
  RS485 counterpart. See `docs/development/LEG_CALIBRATION.md`.
- **Uncalibrated-board marker.** The leg address defaults to `0`, which is
  not a valid RS485 address (the protocol uses 1-6) — it means "never
  assigned an address." A board at address 0 blinks its onboard LED red at
  2 Hz (`status_led.cpp`) and never responds to any RS485 pull (no master
  ever addresses leg 0); the LED goes off immediately once `ADDR <1-6>` is
  run over USB, no reboot required.
- **Interpolation.** LINEAR is the default (easy to verify during bring-up);
  cubic Hermite is enabled via the `INTERP_MODE` parameter.

## Bring-Up Status / TODO

Builds clean; not yet validated on hardware. Known follow-ups:

- Pin map and ADC channel mapping must be **verified against the LegBoard
  schematic** before connecting servos.
- Status LED polarity (`STATUS_LED_ACTIVE_LOW` in `config.h`) is assumed
  common-anode (LOW = on) per the XIAO RP2040's published pinout; **verify on
  real hardware** — invert if the LED behaves backwards (on when calibrated).
- Per-channel current calibration (scale + offset) is persisted in flash and
  defaults to the previous placeholder (`DEFAULT_ISENSE_MA_PER_MV = 1.0`,
  `DEFAULT_ISENSE_OFFSET_MA = 0.0`) until set. The USB calibration command
  set now covers leg address (`ADDR`, `STATUS?`, `PING`), raw pulse-width
  override for bring-up (`PWM`), and current calibration (`CURRAW?`,
  `CURCAL?`, `CURCAL`) — see `tools/leg_configurator.py` /
  `docs/development/LEG_CALIBRATION.md` for the guided address + resistor-based
  calibration wizard that drives these commands directly over the leg's own
  USB serial, or `tools/current_calibration/` for the automated bench
  workflow using the calibration adapter board (`hardware/calboard/`).
- Servo PWM calibration (angle range, neutral) still uses compile-time
  defaults (1000/1500/2000 µs); a full USB calibration tool for servo ranges
  is still planned.
- Cubic interpolation's velocity estimation has untested edge cases (first
  command, post-watchdog restart); prove LINEAR on hardware first.
