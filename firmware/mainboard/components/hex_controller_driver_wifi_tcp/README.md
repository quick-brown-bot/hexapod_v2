# hex_controller_driver_wifi_tcp

## Role
WiFi TCP controller and RPC transport driver.

## Responsibilities
- Start AP-backed TCP server for remote command sessions.
- Accept client connections and stream incoming lines into RPC transport RX queue.
- Register transport sender for outbound RPC responses over active TCP socket.
- Enforce idle timeout and trigger controller failsafe when disconnected.

## Public Surface
- Header: controller_wifi_tcp.h
- Core APIs:
  - controller_driver_init_wifi_tcp
  - controller_wifi_tcp_send_raw

## Integration
- Uses hex_wifi_ap for AP bring-up.
- Uses hex_rpc_transport for inbound/outbound RPC data flow.
- Uses controller_internal helpers for connection and failsafe state.

## SDKConfig Requirements (Current Project)
- WiFi and SoftAP support:
  - CONFIG_ESP_WIFI_ENABLED=y
  - CONFIG_ESP_WIFI_SOFTAP_SUPPORT=y
- Network stack and BSD socket path:
  - CONFIG_ESP_NETIF_TCPIP_LWIP=y
  - CONFIG_ESP_NETIF_USES_TCPIP_WITH_BSD_API=y
  - CONFIG_LWIP_ENABLE=y
- Socket server behavior relies on current lwIP settings:
  - CONFIG_LWIP_MAX_SOCKETS=10
  - CONFIG_LWIP_SO_REUSE=y
  - CONFIG_LWIP_TCPIP_TASK_STACK_SIZE=3072
- AP DHCP service used by this flow:
  - CONFIG_LWIP_DHCPS=y
