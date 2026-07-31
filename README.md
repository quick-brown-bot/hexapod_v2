# Hexapod

An ESP32-powered hexapod robot built as a long-term robotics and embedded systems project.

The goal is not just to make a robot walk, but to learn and at the same time build a platform that is easy to experiment with, tune, and extend. Most robot behavior is configurable at runtime, control inputs are transport-independent, and the motion stack is organized into separated components.

The project combines firmware, hardware design, and system documentation in a single repository.

> 6 legs, 18 servos, ESP32 mainboard, distributed RP2040 leg controllers over
> an RS485 bus, per-servo current sensing.

This is a continuation of the original [Hexabot](https://github.com/quick-brown-bot/hexapod) project, developed here in
[`hexapod_v2`](https://github.com/quick-brown-bot/hexapod_v2). It differs from
the original mainly in electronics architecture — distributed RP2040 leg
controllers over RS485 instead of a single ESP32 driving all servos directly —
and adds new features such as per-servo current sensing.

---

## What Is In This Repository

The repository contains software and hardware assets used to develop and validate the robot.

```text
firmware/
├── mainboard/             ESP32 mainboard firmware (RS485 master)
├── leg/                   RP2040 leg controller firmware
└── mainboard_rs485_test/  Standalone RS485 bring-up/benchmark tool

hardware/
├── mainboard/             MainBoard schematic (ESP32 + IMU + RS485 master)
├── legboard/               LegBoard schematic (RP2040, servo PWM, current sensing)
└── powerboard/            MainPowerBoard schematic (battery, fusing, UBEC/SBEC)

docs/
├── architecture/          System architecture, hardware and mechanics
├── interfaces/            RS485, RPC, WiFi, Bluetooth, controller driver protocols
├── configuration/         Configuration platform design
├── development/           Build, flash, and dev workflow
└── plans/                 Project-wide backlog and research items
```

Mainboard firmware overview: [firmware/mainboard/README.md](firmware/mainboard/README.md)

The main firmware is built around a fixed 100 Hz control loop and a modular motion pipeline:

```text
Controller Input
        ↓
Gait Scheduler
        ↓
Foot Trajectory Generation
        ↓
Inverse Kinematics
        ↓
Motion Limiting
        ↓
Servo Outputs
```

---

## Current Technical Highlights

## Effort Summary

| Effort Area | Scope | Current Status | Notes |
| --- | --- | --- | --- |
| Locomotion Pipeline | Command mapping, gait scheduler, swing trajectory, whole-body IK | Implemented | Runs in a fixed 100 Hz control loop |
| KPP Motion Limits | Velocity, acceleration, jerk limiting and state estimation | Implemented | Runtime limits are configuration-backed |
| Controller Abstraction | Driver-agnostic core plus transport-specific drivers | Implemented | FlySky iBUS, Wi-Fi TCP, and Bluetooth Classic paths are present |
| RPC System | Queue-based command processing and config control | Implemented | Live inspection and runtime tuning are available |
| Configuration Platform | Namespace defaults, migration, discovery, persistence | Implemented | Core runtime namespaces are already consumed by modules |
| Configurator Tooling | External tuning and visualization UX | Planned | Firmware-side groundwork exists; UI work remains |

## Hardware Snapshot

- MCUs: ESP32 mainboard (FreeRTOS via ESP-IDF) + 6 × XIAO RP2040 (one per leg).
- Robot layout: 6 legs, 3 degrees of freedom per leg, 18 hobby servos total.
- Communication: RS485 multi-drop bus connecting the mainboard to all six leg controllers.
- Sensing: INA4181 4-channel current sensing per LegBoard for ground-contact detection.
- Power: dedicated MainPowerBoard with three UBECs (servo) and one SBEC (logic), fully isolated from logic power.
- Mechanical: custom 3D-printed chassis and leg parts.

## High-Level Architecture

```mermaid
flowchart LR
        Control[Controller stack]
        Config[RPC and config]
        App[Application loop]
        Loc[Locomotion]
        Limits[Motion limiting]
        Act[Actuation]
        Robot[Robot hardware]

        Control --> App
        Config --> App
        App --> Loc --> Limits --> Act --> Robot
        Config --> Control
```

- Controller stack: receives operator input from supported transports and normalizes it.
- RPC and config: handles runtime commands, parameter discovery, persistence, and save flows.
- Application loop: owns boot order and the 100 Hz control cycle.
- Locomotion: turns operator intent into gait phase, foot targets, and joint-space commands.
- Motion limiting: constrains commanded motion to configured dynamic envelopes.
- Actuation: maps joint commands to the PWM outputs that drive the robot.

### Runtime Configuration

Configuration is treated as a first-class subsystem rather than a collection of compile-time constants.

Parameters are organized into namespaces. Each namespace owns:

- parameter definitions,
- default values,
- validation rules,
- persistence behavior,
- migration logic,
- RPC discovery metadata.

The configuration manager provides:

- runtime parameter inspection,
- runtime modification,
- save/load operations,
- namespace registration,
- NVS persistence,
- version-aware migration support.

Robot geometry, calibration data, controller settings, motion limits, and other runtime behavior are loaded through this system rather than being hardcoded throughout the codebase.

### Motion Pipeline

The locomotion stack converts user commands into joint targets through a sequence of dedicated subsystems:

- gait scheduling,
- swing trajectory generation,
- inverse kinematics,
- velocity limiting,
- acceleration limiting,
- jerk limiting,
- servo actuation.

The motion loop runs at 100 Hz.

### Controller Abstraction

Controller transports are separated from locomotion.

Supported ingress paths currently include:

- FlySky iBUS,
- Wi-Fi TCP,
- Bluetooth Classic.

Each transport driver translates incoming data into a normalized controller state consumed by the motion system.

This allows locomotion, gait generation, and motion limiting to remain independent of transport-specific protocols.

### Hardware-Aware Design

The robot uses 18 servos arranged as three joints per leg:

- Coxa
- Femur
- Tibia

Servo signal generation is implemented on ESP32 using a combination of MCPWM and LEDC peripherals to fit within available hardware resources.

Power for logic and servos is separated to improve stability under heavy load.

---

## Engineering Problems Being Explored

This project is primarily a learning and engineering platform.

Current focus areas include:

- real-time control on resource-constrained hardware,
- gait generation and scheduling,
- inverse kinematics,
- motion smoothing and trajectory constraints,
- configuration architecture for embedded systems,
- transport-independent control interfaces,
- robotics software maintainability,
- practical servo-driven locomotion.

---

## Hardware Architecture

Distributed leg controllers with full separation of logic and servo power. The
architecture is defined and schematics are authored; firmware is in development.

- **Three boards**: MainBoard (ESP32 + IMU + RS485 master), LegBoard × 6 (XIAO
  RP2040, servo PWM, per-servo current sensing), MainPowerBoard (battery,
  fusing, UBEC/SBEC distribution).
- **The ESP32 does not generate servo PWM.** Joint commands are sent over RS485;
  each RP2040 generates PWM locally and interpolates between targets.
- **Per-servo current sensing** on every LegBoard enables ground-contact
  detection from current spikes without dedicated force sensors.
- **Servo current is fully isolated from the MainBoard** at the hardware level.

See [`docs/architecture/`](docs/architecture/) for the full architecture and interface documentation.

---

## Documentation

The repository contains detailed documentation covering implementation details and architecture decisions.

- [docs/architecture/SYSTEM_ARCHITECTURE.md](docs/architecture/SYSTEM_ARCHITECTURE.md)
- [docs/architecture/HARDWARE_AND_MECHANICS.md](docs/architecture/HARDWARE_AND_MECHANICS.md)
- [docs/interfaces/RS485_PROTOCOL.md](docs/interfaces/RS485_PROTOCOL.md)
- [docs/development/README.md](docs/development/README.md)
- [docs/interfaces/RPC_USER_GUIDE.md](docs/interfaces/RPC_USER_GUIDE.md)
- [docs/configuration/CONFIGURATION_PERSISTENCE_DESIGN.md](docs/configuration/CONFIGURATION_PERSISTENCE_DESIGN.md)
- [firmware/mainboard/README.md](firmware/mainboard/README.md)

Full documentation index: [docs/README.md](docs/README.md)

---

## Project Status

The project is under active development.

Current work focuses on improving locomotion quality, refining control architecture boundaries, and preparing the next generation of leg electronics while keeping the existing robot operational as a test platform.

---

## License

This repository uses Apache License 2.0 for the mainboard firmware.

License file location: [LICENSE](LICENSE)

Include the following notice in derivative works:

```text
Copyright 2025 Pawel Grudzien

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```
