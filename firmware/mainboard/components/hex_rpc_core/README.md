# hex_rpc_core

## Role
RPC command parsing, command dispatch, and RPC processing task orchestration.

## Responsibilities
- Parse inbound ASCII RPC commands and tokenize arguments.
- Dispatch to config, controller, actuation/IK diagnostic, and system command handlers.
- Run the RPC processing task that consumes transport RX messages.
- Emit responses through registered transport send callback or transport TX queue.

## Public Surface
- Header: rpc_commands.h
- Core APIs:
  - rpc_init
  - rpc_feed_bytes
  - rpc_poll
  - rpc_set_send_callback
- Commands: `get`/`set`/`setpersist`/`export`/`save`/`list`/`factory-reset` (config),
  `set controller` (high-rate channel stream), `help`/`version`, and two RS485
  bring-up/diagnostic commands that bypass gait/IK via `hex_actuation`'s manual
  per-leg override:
  - `joint <leg 1-6> <coxa_deg> <femur_deg> <tibia_deg>` / `joint <leg> release`
  - `ik <leg 1-6> <x_m> <y_m> <z_m>` (leg-local Cartesian target -> IK -> same override)

## Integration
- Consumes inbound bytes/messages from hex_rpc_transport.
- Uses hex_config_manager for configuration get/set/list/save operations, including
  the `joint_cal` namespace's per-joint `min_rad`/`max_rad` as the range clamp for
  the `joint`/`ik` commands above (not a locally-invented constant).
- Uses controller internal channel update path for high-rate controller set commands.
- Uses hex_actuation's manual joint override and hex_robot_config's per-leg IK handle
  for the `joint`/`ik` diagnostic commands.

## SDKConfig Requirements (Current Project)
- No dedicated module-specific Kconfig toggles are required.
- Depends on project FreeRTOS support and task creation configuration.
