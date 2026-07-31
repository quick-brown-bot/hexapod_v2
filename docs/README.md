# Hexapod Documentation

## Structure

```
docs/
  architecture/    — system architecture, hardware and mechanics
  interfaces/      — RS485, RPC, WiFi, Bluetooth, controller driver protocols
  configuration/   — configuration platform design
  development/     — build, flash, and dev workflow
  plans/           — project-wide TODO and forward-looking plans
```

---

## Architecture

Hardware: [`hardware/`](../hardware/README.md).
Firmware: [`firmware/`](../firmware/leg/).

- [architecture/SYSTEM_ARCHITECTURE.md](architecture/SYSTEM_ARCHITECTURE.md)
- [architecture/HARDWARE_AND_MECHANICS.md](architecture/HARDWARE_AND_MECHANICS.md)

## Interfaces

- [interfaces/RS485_PROTOCOL.md](interfaces/RS485_PROTOCOL.md)
- [interfaces/RPC_USER_GUIDE.md](interfaces/RPC_USER_GUIDE.md)
- [interfaces/RPC_SYSTEM_DESIGN.md](interfaces/RPC_SYSTEM_DESIGN.md)
- [interfaces/CONTROLLER_DRIVERS.md](interfaces/CONTROLLER_DRIVERS.md)
- [interfaces/WIFI_TCP_PROTOCOL.md](interfaces/WIFI_TCP_PROTOCOL.md)
- [interfaces/WIFI_NETWORK_MODES.md](interfaces/WIFI_NETWORK_MODES.md)
- [interfaces/BLUETOOTH_CLASSIC_PROTOCOL.md](interfaces/BLUETOOTH_CLASSIC_PROTOCOL.md)

## Configuration

- [configuration/CONFIGURATION_PERSISTENCE_DESIGN.md](configuration/CONFIGURATION_PERSISTENCE_DESIGN.md)
- [configuration/CONFIG_MANAGER_NAMESPACE_TEMPLATE.md](configuration/CONFIG_MANAGER_NAMESPACE_TEMPLATE.md)

## Development

- [development/README.md](development/README.md)

---

## Plans

- [plans/TODO.md](plans/TODO.md) — project-wide feature backlog and research items
