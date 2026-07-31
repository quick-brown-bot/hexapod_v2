# hex_rpc_core

## Role
RPC command parsing, command dispatch, and RPC processing task orchestration.

## Responsibilities
- Parse inbound ASCII RPC commands and tokenize arguments.
- Dispatch to config, controller, and system command handlers.
- Run the RPC processing task that consumes transport RX messages.
- Emit responses through registered transport send callback or transport TX queue.

## Public Surface
- Header: rpc_commands.h
- Core APIs:
  - rpc_init
  - rpc_feed_bytes
  - rpc_poll
  - rpc_set_send_callback

## Integration
- Consumes inbound bytes/messages from hex_rpc_transport.
- Uses hex_config_manager for configuration get/set/list/save operations.
- Uses controller internal channel update path for high-rate controller set commands.

## SDKConfig Requirements (Current Project)
- No dedicated module-specific Kconfig toggles are required.
- Depends on project FreeRTOS support and task creation configuration.
