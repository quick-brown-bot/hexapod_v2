# Development Setup — V2

## Purpose

This document is the software-side starting point for V2 development. V2
involves two separate firmware projects and two toolchains, and hardware
bring-up must follow a specific sequence because the mainboard and six
LegBoards depend on each other over RS485.

---

## Reading Before Starting

Read these first:

1. [../architecture/SYSTEM_ARCHITECTURE.md](../architecture/SYSTEM_ARCHITECTURE.md) — component split and RS485 communication architecture
2. [../architecture/HARDWARE_AND_MECHANICS.md](../architecture/HARDWARE_AND_MECHANICS.md) — board responsibilities, power topology, wiring
3. [../interfaces/RS485_PROTOCOL.md](../interfaces/RS485_PROTOCOL.md) — transaction model and frame format

---

## Repository Orientation

```
firmware/
  v2/
    mainboard/       ESP32 firmware — motion control, RS485 master, RPC, config
    leg/             RP2040 firmware — servo PWM, interpolation, current sensing

docs/
  v2/
    architecture/    SYSTEM_ARCHITECTURE.md, HARDWARE_AND_MECHANICS.md
    interfaces/      RS485_PROTOCOL.md
    development/     This file

hardware/
  v2/
    mainboard/       ESP32 MainBoard schematic (Python source + KiCad output)
    legboard/        RP2040 LegBoard schematic
    powerboard/      MainPowerBoard schematic
```

---

## Development Environment

V2 requires two toolchains side by side.

### Mainboard — ESP32 (ESP-IDF)

Identical to V1.

- ESP-IDF 5.x
- Visual Studio Code with the official ESP-IDF extension
- Target: ESP32

Setup:
1. Install ESP-IDF 5.x using the official Espressif tooling.
2. Install the ESP-IDF VS Code extension.
3. Configure the extension with the ESP-IDF tools path and Python environment.

### Leg — RP2040 (PlatformIO + arduino-pico)

- PlatformIO with the VS Code PlatformIO extension
- Project root: `firmware/leg/`
- Framework: **arduino-pico** (Earle Philhower's Arduino core for RP2040)

arduino-pico sits directly on top of the Pico SDK — native `pico/stdlib.h`,
`hardware/uart.h`, `hardware/pwm.h`, and other SDK headers are fully accessible
alongside it. This gives Pico SDK-level control over UART timing, PWM, and DMA
while keeping the PlatformIO build system and upload tooling working.

Setup:
1. Install the PlatformIO VS Code extension.
2. Open `firmware/leg/` as a PlatformIO project.
3. PlatformIO installs the `raspberrypi` platform and arduino-pico core
   automatically on first build.

Both extensions can coexist in the same VS Code instance. Use the workspace
switcher or terminal context to target the correct project.

---

## Mainboard Firmware

Firmware lives at `firmware/mainboard/`. The RS485 distributed-leg design adds
`hex_rs485_master` and a modified `hex_actuation` that writes to the command
buffer instead of PWM registers.

### Build, Flash, Monitor

From an ESP-IDF terminal in `firmware/mainboard/`:

```bash
idf.py build
idf.py flash
idf.py monitor
```

The serial monitor shows boot logs, RS485 master task status, and any leg
communication events. Keep it open during RS485 bring-up.

### Host-Side Tests

The host-side pytest suite tests RPC and config flows over Wi-Fi.

```bash
pip install -r firmware/mainboard/test/requirements.txt
python -m pytest -q -p no:embedded firmware/mainboard/test/test_config_general_listing.py
```

Set the robot endpoint if not using defaults:

```bash
set HEXAPOD_IP=<robot_ip>
set HEXAPOD_PORT=<robot_port>
```

---

## Leg Firmware

Firmware lives at `firmware/leg/`. One binary is flashed to all six
LegBoards.

### Leg Addressing

Each RP2040 stores its leg address (1–6) in flash. The address is written
during the pre-mount calibration step, before the leg is physically attached
to the robot frame. All six LegBoards run identical firmware; the address
is the only per-board difference.

Calibration is performed over USB while the leg is on the bench. A
calibration tool (Python script over USB serial) connects to the RP2040,
writes the address and initial servo parameters to flash, and verifies the
stored values. Once calibrated and mounted, the leg uses RS485 exclusively
— USB is only needed for re-calibration or firmware updates.

See [Phase 3 in the bring-up sequence](#phase-3--first-leg-rj11-only-no-servo-power)
for how to verify the address is correctly set before mounting.

### Build

From a PlatformIO terminal in `firmware/leg/`:

```bash
pio run
```

### Flash

The XIAO RP2040 supports two flashing methods:

**UF2 drag-and-drop (recommended for bring-up):**
1. Hold the BOOTSEL button on the XIAO RP2040 while connecting USB.
2. A USB mass storage device (`RPI-RP2`) appears.
3. Copy the `.uf2` file from `.pio/build/<env>/` onto the drive.
4. The device reboots and runs the new firmware.

**PlatformIO upload (once the build environment is stable):**
```bash
pio run --target upload
```

### Monitor

```bash
pio device monitor --baud 115200
```

The leg firmware should log its address, parameter defaults, and RS485 receive
events on startup.

---

## Bring-Up Sequence

Bring up hardware in this order. Each phase must pass before the next begins.

### Phase 1 — MainPowerBoard (no MCUs connected)

1. Connect the battery through the master power switch.
2. Verify SBEC output: +5 V on the logic output header.
3. Verify UBEC outputs: +6 V on each leg servo power terminal.
4. Confirm all status LEDs (UBEC 1/2/3, SBEC) are lit.
5. Power off before connecting any MCU boards.

### Phase 2 — Mainboard Alone

1. Connect only the SBEC +5 V logic supply to the MainBoard.
2. Flash mainboard firmware over USB.
3. Open serial monitor. Confirm:
   - Config manager initializes.
   - RS485 master task starts.
   - Wi-Fi AP comes up (SSID visible on a nearby device).
4. Connect over Wi-Fi, run an RPC command to confirm the RPC layer is live.
5. At this point the mainboard will be logging RS485 timeouts for all six legs
   — this is expected. Confirm the timeout handling produces stale-entry markers
   rather than crashes.

### Phase 3 — First Leg (RJ11 only, no servo power)

1. Flash one RP2040 LegBoard with the correct address set (see Leg Addressing).
2. Connect the RJ11 cable between MainBoard port 1 and the LegBoard.
3. **Do not connect servo power yet.**
4. Open serial monitor on the RP2040 (USB). Confirm it is receiving RS485
   frames and responding.
5. On the mainboard monitor, confirm the telemetry buffer entry for leg 1 is
   updating (no longer stale).
6. Send a pull with `JOINT_POS` flag set and verify current joint positions
   appear in the telemetry response.

### Phase 4 — Servo Power for First Leg

1. Connect the MainPowerBoard servo output for legs 1/2 to the LegBoard.
2. Command a small angle change via RPC.
3. Verify the servo moves.
4. Verify coxa/femur/tibia current readings in telemetry change under load.

### Phase 5 — Remaining Legs

Repeat phases 3 and 4 for legs 2 through 6. Add one leg at a time. Verify
RS485 addressing is correct for each (the wrong address will produce no
response and a timeout on the master).

---

## RS485 Debugging

The RS485 bus carries text frames at 1 Mbps. It can be monitored directly
with a USB-to-RS485 adapter connected to the A/B lines and a terminal set to
1 000 000 baud.

Pull frames start with `>`, telemetry responses start with `<`. Frames end
with `\n` and carry a `*XX` CRC8 suffix. A serial monitor with hex display
helps verify CRC and frame boundaries.

Common failure modes:

| Symptom | Likely cause |
|---------|--------------|
| All legs stale, no responses | Wiring (A/B swapped, no ground, missing power to LegBoard) |
| One leg stale, others OK | Wrong address on that LegBoard, or faulty RJ11 connection |
| Intermittent CRC failures | Bus termination missing or wrong, DE turnaround too short |
| Leg responds once then goes silent | RP2040 watchdog triggering, or DE pin misconfigured on LegBoard |

---

## Practical First Steps

For a new V2 contributor:

1. Read the three architecture docs listed at the top of this file.
2. Set up both toolchains (ESP-IDF and PlatformIO) and verify each builds.
3. Flash the mainboard, confirm it boots to phase 2 state above.
4. Flash one leg, connect it, reach phase 3 state.
5. Only then start modifying firmware — you have a working reference baseline.

---

## Related Docs

- [../architecture/SYSTEM_ARCHITECTURE.md](../architecture/SYSTEM_ARCHITECTURE.md)
- [../architecture/HARDWARE_AND_MECHANICS.md](../architecture/HARDWARE_AND_MECHANICS.md)
- [../interfaces/RS485_PROTOCOL.md](../interfaces/RS485_PROTOCOL.md)
- [../interfaces/RPC_USER_GUIDE.md](../interfaces/RPC_USER_GUIDE.md)
- [../configuration/CONFIGURATION_PERSISTENCE_DESIGN.md](../configuration/CONFIGURATION_PERSISTENCE_DESIGN.md)
- [../../hardware/README.md](../../hardware/README.md)
