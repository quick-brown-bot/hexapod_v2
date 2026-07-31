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
  persist.h/.cpp Flash-backed leg address (arduino-pico EEPROM)
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
- **Persistence split.** Only the leg address is stored in flash (it must differ
  per board and survive reboot). Runtime parameters (move duration, watchdog
  timeout, interpolation mode, joint limits) default in firmware and are
  re-applied by the ESP32 on recovery — matching the protocol doc's
  "simplest first" approach.
- **Interpolation.** LINEAR is the default (easy to verify during bring-up);
  cubic Hermite is enabled via the `INTERP_MODE` parameter.

## Bring-Up Status / TODO

Builds clean; not yet validated on hardware. Known follow-ups:

- Pin map and ADC channel mapping must be **verified against the LegBoard
  schematic** before connecting servos.
- `ISENSE_MA_PER_MV` is a placeholder; calibrate against a measured load.
- Servo PWM calibration uses compile-time defaults (1000/1500/2000 µs); a full
  USB calibration tool (servo ranges, current scale) is planned. The calibration
  command set today covers only the leg address (`ADDR`, `STATUS?`, `PING`).
- Cubic interpolation's velocity estimation has untested edge cases (first
  command, post-watchdog restart); prove LINEAR on hardware first.
