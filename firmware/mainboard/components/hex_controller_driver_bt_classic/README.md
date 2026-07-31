# hex_controller_driver_bt_classic

## Role
Bluetooth Classic SPP controller and RPC transport driver.

## Responsibilities
- Initialize Bluetooth Classic stack and SPP server.
- Handle pairing/authentication callbacks and connection lifecycle.
- Forward incoming SPP payloads into RPC transport RX queue.
- Register transport sender for outbound RPC responses over SPP.
- Trigger failsafe on disconnect or timeout conditions.

## Public Surface
- Header: controller_bt_classic.h
- Core APIs:
  - controller_driver_init_bt_classic
  - controller_bt_classic_send_raw
  - controller_bt_get_device_name

## Integration
- Registered by controller core through driver selection.
- Uses hex_rpc_transport for RPC message exchange.
- Uses controller_internal helpers for shared connection and failsafe behavior.

## SDKConfig Requirements (Current Project)
- Bluetooth stack enabled:
  - CONFIG_BT_ENABLED=y
  - CONFIG_BT_BLUEDROID_ENABLED=y
  - CONFIG_BLUEDROID_ENABLED=y
- Classic Bluetooth mode enabled:
  - CONFIG_BT_CLASSIC_ENABLED=y
  - CONFIG_CLASSIC_BT_ENABLED=y
  - CONFIG_BTDM_CTRL_MODE_BR_EDR_ONLY=y
  - CONFIG_BTDM_CONTROLLER_MODE_BR_EDR_ONLY=y
- SPP profile enabled:
  - CONFIG_BT_SPP_ENABLED=y
- Controller and Bluedroid task affinity and stacks currently configured as:
  - CONFIG_BT_BTC_TASK_STACK_SIZE=3072
  - CONFIG_BT_BTU_TASK_STACK_SIZE=4352
  - CONFIG_BTDM_CTRL_PINNED_TO_CORE=0
  - CONFIG_BLUEDROID_PINNED_TO_CORE=0
