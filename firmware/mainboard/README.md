<div align="center">

# Hexapod Mainboard Firmware (ESP32)

| Supported Targets | ESP32 |
| ----------------- | ----- |

</div>

## Overview

This directory contains ESP-IDF firmware for the hexapod mainboard. It provides the core locomotion runtime, controller ingestion, transport-facing RPC path, and namespace-backed runtime configuration.

## Scope

- Fixed-step 100 Hz locomotion control loop.
- Motion limiting and servo actuation for the 18-DOF platform.
- Multi-transport controller ingress (FlySky iBUS, Wi-Fi TCP, Bluetooth Classic).
- Runtime configuration through hex_config_manager namespaces.

## Directory Map

- main/: top-level bootstrapping and loop orchestration.
- components/: runtime modules split by subsystem.
- test/: host-side integration tests for RPC/config behavior.
- ../../docs/: architecture and protocol documentation shared across the monorepo.

## Mainboard Docs

- Documentation hub: [../../docs/README.md](../../docs/README.md)
- Architecture: [../../docs/architecture/SYSTEM_ARCHITECTURE.md](../../docs/architecture/SYSTEM_ARCHITECTURE.md)
- Configuration persistence: [../../docs/configuration/CONFIGURATION_PERSISTENCE_DESIGN.md](../../docs/configuration/CONFIGURATION_PERSISTENCE_DESIGN.md)
- RPC user guide: [../../docs/interfaces/RPC_USER_GUIDE.md](../../docs/interfaces/RPC_USER_GUIDE.md)

## Build And Flash

Run from this directory in an ESP-IDF-enabled terminal:

```bash
idf.py build
idf.py flash
idf.py monitor
```

## Host-Side Tests

Install requirements:

```bash
pip install -r test/requirements.txt
```

Run one focused test module:

```bash
python -m pytest -q -p no:embedded test/test_config_general_listing.py
```

## License

Apache License 2.0. See [LICENSE](../../LICENSE).
