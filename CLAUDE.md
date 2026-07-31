# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repo is

`hexapod_v2` is a continuation of the original [Hexabot](https://github.com/quick-brown-bot/hexapod)
project. The original repo used a single ESP32 driving all 18 servos directly
by PWM. This repo redesigns the electronics around **three separate PCBs**
and a distributed control architecture, and adds per-servo current sensing.
There is no "V1" in this repo anymore — it was pruned out; everything here is
the current (and only) design.

Three boards, one robot:

- **MainBoard** — ESP32. Owns locomotion, controller input, RPC, config, and
  is the RS485 bus master. Does **not** generate servo PWM itself.
- **LegBoard** ×6 — XIAO RP2040, one per leg. RS485 slave. Receives joint
  targets, interpolates locally, drives 3 servos, samples per-servo current.
- **MainPowerBoard** — battery → fuse/switch/reverse-polarity protection →
  3× UBEC (servo power) + 1× SBEC (logic power), fully separated rails.

The MainBoard and LegBoards talk over a multi-drop RS485 bus: the ESP32 polls
each leg in turn (send targets, get telemetry back) inside a dedicated
FreeRTOS task, decoupled from the 100 Hz locomotion loop.

## Repo layout

```
firmware/
  mainboard/            ESP32 firmware, ESP-IDF. The main robot brain.
  leg/                  RP2040 firmware, PlatformIO. Runs on every LegBoard.
  mainboard_rs485_test/ Standalone ESP-IDF app for RS485 bring-up/benchmarking
                        (not part of the mainboard build).

hardware/
  mainboard/  legboard/  powerboard/   One folder per board: <board>_sch.py
                                        (source of truth) + generated .kicad_sch/.kicad_pcb
  schematic/            The schematic-as-code Python toolchain itself (DSL, not board data)
  symbols/  footprints/  models/       Shared KiCad libraries + FreeCAD model, self-contained
  archive/               Gerber/BOM zips per fabrication order

docs/
  architecture/    System architecture, hardware/mechanics
  interfaces/      Wire protocols: RS485, RPC, WiFi, Bluetooth, controller drivers
  configuration/   Runtime config platform design
  development/     Toolchain setup and bring-up sequence
  plans/           Backlog / TODO
```

**When asked to change legboard or mainboard behavior**, the split is:
firmware logic → `firmware/leg/` or `firmware/mainboard/`; electrical/pinout
changes → `hardware/legboard/legboard_sch.py` or `hardware/mainboard/mainboard_sch.py`.
Check `docs/architecture/HARDWARE_AND_MECHANICS.md` for the pin map before
touching either, since firmware pin assignments must match the schematic.

## Firmware architecture (mainboard, ESP32/ESP-IDF)

`firmware/mainboard/main/` boots the system and owns the 100 Hz loop.
`firmware/mainboard/components/` holds one ESP-IDF component per subsystem:

- **Motion**: `hex_locomotion` (gait/trajectory) → `hex_motion_limits`
  (velocity/accel/jerk limiting, KPP) → `hex_actuation` (writes to a shared
  command buffer instead of touching servo hardware) → `hex_kinematics`
  (math-only IK), `hex_robot_config` (geometry/calibration).
- **RS485 comm**: `hex_rs485_master` — the only component allowed to touch
  the bus. Runs its own task; reads the command buffer, writes a telemetry
  buffer the motion stack reads back. This is the layer that replaced direct
  PWM in the original design.
- **Controller/transport**: `hex_controller_core` plus one driver per input
  path (`hex_controller_driver_flysky_ibus`, `_wifi_tcp`, `_bt_classic`),
  `hex_wifi_ap`.
- **RPC/config**: `hex_rpc_core`, `hex_rpc_transport`, `hex_config_manager`,
  `hex_shared_types`. All runtime-tunable values are namespace-backed in
  `hex_config_manager` — there is no consumer-local fallback/default; see
  `docs/configuration/CONFIGURATION_PERSISTENCE_DESIGN.md` and
  `firmware/mainboard/AGENTS.md` for the contract before adding a new
  tunable.

Full agent-facing conventions (component README requirements, config
contract, testing principles) already live in `firmware/mainboard/AGENTS.md`
and `firmware/mainboard/.github/instructions/` — read those before editing
mainboard components, this file intentionally doesn't restate them.

## Firmware architecture (leg, RP2040/PlatformIO)

`firmware/leg/src/` — one binary flashed to all six LegBoards; each is
distinguished only by an RS485 address persisted in flash (`persist.*`).
Module split: `protocol.*` (frame parse/build + CRC-8/SMBus, host-testable,
no hardware deps — must stay byte-for-byte compatible with
`hex_rs485_master` on the mainboard side), `rs485.*` (manual half-duplex DE
turnaround — arduino-pico has no hardware auto-DE), `interp.*` (linear or
cubic-Hermite interpolation between targets), `servo.*` (50 Hz hardware PWM),
`current.*` (INA4181 sensing via RP2040 ADC), `calib.*` (USB-serial bring-up:
set leg address). The LegBoard never speaks unless spoken to — on CRC
failure or address mismatch it stays silent; the master's timeout is the
only error signal.

## Build / test commands

Mainboard (from `firmware/mainboard/`, ESP-IDF environment):
```bash
idf.py build && idf.py flash && idf.py monitor
pip install -r test/requirements.txt
python -m pytest -q -p no:embedded test/test_config_general_listing.py   # host-side RPC/config tests, single module
```

Leg (from `firmware/leg/`, PlatformIO):
```bash
pio run                    # build
pio run --target upload    # flash
pio device monitor         # USB serial
```

RS485 bring-up/benchmark tool (`firmware/mainboard_rs485_test/`) builds and
flashes the same way as mainboard (`idf.py build/flash/monitor`), but is a
separate standalone app, not linked into the mainboard build.

Hardware schematics (from repo root):
```bash
python hardware/mainboard/mainboard_sch.py
python hardware/legboard/legboard_sch.py
python hardware/powerboard/powerboard_sch.py
```

## Hardware: schematic-as-code

Every board's `.kicad_sch` is **generated** from a same-named `_sch.py` —
never hand-edit a `.kicad_sch`, edit the `.py` and rerun it. `uuids.json` per
board is the committed UUID registry that keeps KiCad symbol UUIDs stable
across regeneration, which is what protects PCB layout/routing from being
lost when the schematic changes. The `.claude/skills/hardware-schematics`
skill and `hardware/schematic/README.md` cover the full DSL and workflow —
use the skill rather than editing schematics by hand or re-deriving the
rules here.

## Documentation map

Don't duplicate these — read them for their topic instead:

- **System architecture / component wiring**: `docs/architecture/SYSTEM_ARCHITECTURE.md`
- **Board responsibilities, power topology, pinout**: `docs/architecture/HARDWARE_AND_MECHANICS.md`
- **RS485 wire protocol** (frame format, CRC, timing budget): `docs/interfaces/RS485_PROTOCOL.md`
- **RPC protocol / user guide**: `docs/interfaces/RPC_SYSTEM_DESIGN.md`, `docs/interfaces/RPC_USER_GUIDE.md`
- **WiFi/Bluetooth/controller transports**: `docs/interfaces/WIFI_*.md`, `docs/interfaces/BLUETOOTH_CLASSIC_PROTOCOL.md`, `docs/interfaces/CONTROLLER_DRIVERS.md`
- **Config namespace/persistence design**: `docs/configuration/CONFIGURATION_PERSISTENCE_DESIGN.md`, `docs/configuration/CONFIG_MANAGER_NAMESPACE_TEMPLATE.md`
- **Dev environment setup, bring-up sequence, troubleshooting**: `docs/development/README.md`
- **Backlog**: `docs/plans/TODO.md`
- Full index with descriptions: `docs/README.md`

## Key repo-wide conventions

- Board schematics: edit `.py`, never `.kicad_sch`; commit `uuids.json`
  changes as additions only, never modified UUID values.
- Mainboard: new tunables go through `hex_config_manager` namespaces, not
  local defaults/fallbacks.
- Leg protocol changes must be mirrored in both `firmware/leg/src/protocol.*`
  and `firmware/mainboard/components/hex_rs485_master/` — they encode/decode
  the same wire format independently.
