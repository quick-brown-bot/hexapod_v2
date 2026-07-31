# Hexapod RPC User Guide

## Overview

The Hexapod firmware exposes a text-based RPC interface for runtime configuration and control ingress.

This guide is operational-only:
- how to connect
- how to send commands safely
- practical workflows for tuning and persistence

For authoritative protocol behavior (supported commands, framing, and semantics), see [RPC_SYSTEM_DESIGN.md](RPC_SYSTEM_DESIGN.md).

## Transport Setup

### Bluetooth Classic SPP

1. Power on the robot.
2. Pair with device name `HEXAPOD_XXXXXX` (MAC suffix).
3. Use PIN `1234` unless changed.
4. Open the outgoing virtual COM port in your terminal.

Notes:
- One SPP client is supported at a time.
- Transport is a byte stream; send text commands terminated with newline.

### WiFi TCP

1. Connect to the robot AP.
2. Open a TCP connection to the configured listen port.
3. Send line-based text commands terminated with `\n` or `\r\n`.

Notes:
- One active TCP client session is supported at a time.

## Command Format

General form (refer to canonical contract for full command semantics):

```text
<command> [arg1] [arg2] ...
```

Examples:

```text
list namespaces
list system
get system emergency_stop_enabled
set system emergency_stop_enabled true
setpersist wifi tcp_listen_port 5555
save wifi
export system
```

High-rate controller ingress:

```text
set controller <ch0> <ch1> ... <ch31>
```

If fewer than 32 channel values are provided, missing channels are set to `0`.

Tip:
- Run `help` and `list namespaces` on the connected target to discover runtime-supported operations.

## Response Behavior

- Responses are CRLF-terminated text lines.
- Current implementation does not append a separate trailing `OK` line.
- Most commands return one line with either data or an `ERROR ...` message.
- `set controller ...` intentionally returns no response to avoid extra traffic.

Typical responses:

```text
namespaces: system, joint_cal, leg_geom, motion_lim, controller, wifi
emergency_stop_enabled=true
emergency_stop_enabled=true (persist)
save system: OK
ERROR set emergency_stop_enabled
```

## Limits and Validation

- Maximum input line length is 255 characters in the RPC parser.
- Overlong lines return `ERROR line too long`.
- Parameters are type-checked and range-validated by config manager descriptors.

## Practical Workflows

### Read and tune one parameter

```text
get motion_lim max_velocity_coxa
set motion_lim max_velocity_coxa 4.5
setpersist motion_lim max_velocity_coxa 4.5
```

### Tune gait trajectory parameters

```text
list gait
get gait cycle_time_s
get gait step_length_m
set gait step_length_m 0.09
setpersist gait max_yaw_per_cycle_rad 0.35
setpersist gait turn_direction 1.0
save gait
```

Notes:
- `cycle_time_s`, `step_length_m`, and `clearance_height_m` control high-level gait timing and swing shape.
- `y_range_m`, `z_min_m`, and `z_max_m` control pose-to-foot target mapping.
- `max_yaw_per_cycle_rad` and `turn_direction` control turning behavior.

### Tune controller stick deadband

```text
get controller stick_deadband
set controller stick_deadband 0.03
setpersist controller stick_deadband 0.03
save controller
```

Notes:
- `stick_deadband` is clamped and validated in range `0.0 .. 0.3`.
- Lower values increase stick sensitivity near center; higher values suppress noise.

### Tune controller failsafe command profile

```text
list controller
get controller failsafe_enable
set controller failsafe_enable 1
set controller failsafe_gait 2
set controller failsafe_step_scale 0.4
setpersist controller failsafe_vx 0.0
setpersist controller failsafe_wz 0.0
setpersist controller failsafe_z_target -0.1
setpersist controller failsafe_pose_mode 1
save controller
```

Notes:
- Failsafe profile is applied when controller input is unavailable.
- `failsafe_gait` values are `0=wave`, `1=ripple`, `2=tripod`.
- Float constraints: `failsafe_vx`, `failsafe_wz`, `failsafe_z_target`, `failsafe_y_offset` in `-1.0 .. 1.0`, `failsafe_step_scale` in `0.0 .. 1.0`.

### Save all dirty namespaces

```text
save
```

### Factory reset

```text
factory-reset
```

This erases robot configuration namespaces and reloads defaults. Use with care.

## Troubleshooting

- No response:
  - verify transport connection (Bluetooth COM/TCP socket)
  - verify newline-terminated commands
- `unknown: <cmd>`:
  - command not implemented
- `set: not found` or `get: not found`:
  - wrong namespace or parameter name
- frequent disconnects:
  - check WiFi/Bluetooth link quality and timeout settings

## Related Documentation

- See [RPC_SYSTEM_DESIGN.md](RPC_SYSTEM_DESIGN.md) for the canonical RPC command contract.
- See [WIFI_TCP_PROTOCOL.md](WIFI_TCP_PROTOCOL.md) and [BLUETOOTH_CLASSIC_PROTOCOL.md](BLUETOOTH_CLASSIC_PROTOCOL.md) for transport-specific context.
