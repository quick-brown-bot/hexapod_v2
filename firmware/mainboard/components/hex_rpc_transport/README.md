# hex_rpc_transport

## Role
Queue-based transport abstraction for RPC message flow.

## Responsibilities
- Provide a unified interface for inbound RPC bytes from different transports.
- Queue outbound RPC responses and dispatch through registered transport senders.
- Isolate RPC command processing from transport-specific I/O details.

## Public Surface
- Header: rpc_transport.h
- Core APIs:
  - rpc_transport_init
  - rpc_transport_rx_send
  - rpc_transport_rx_receive
  - rpc_transport_tx_send
  - rpc_transport_register_sender

## Integration
- Producers (controller transport drivers) push incoming bytes to the RX queue.
- RPC command layer consumes RX queue messages and pushes responses to TX queue.
- Transport sender callbacks deliver TX data over Bluetooth, WiFi TCP, serial, or internal channels.

## SDKConfig Requirements (Current Project)
- No transport-specific Kconfig flag is required by this module itself.
- This module depends on FreeRTOS task and queue support provided by the current project configuration, including:
  - CONFIG_FREERTOS_HZ=100
  - CONFIG_FREERTOS_NUMBER_OF_CORES=2
