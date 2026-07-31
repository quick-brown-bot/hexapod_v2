# hex_wifi_ap

## Role
WiFi Access Point bootstrap utility for local robot connectivity.

## Responsibilities
- Initialize WiFi AP mode and network stack once.
- Build AP SSID using fixed, MAC-suffix, or random-suffix strategies.
- Apply AP options such as password, channel, and max clients.
- Expose active SSID for diagnostics and user feedback.

## Public Surface
- Header: wifi_ap.h
- Core APIs:
  - wifi_ap_init_with_options
  - wifi_ap_init_once
  - wifi_ap_get_ssid

## Integration
- Used by the WiFi TCP controller driver to provide an AP for command/control sessions.
- Keeps AP bring-up logic outside controller and RPC core modules.

## SDKConfig Requirements (Current Project)
- WiFi stack enabled:
  - CONFIG_ESP_WIFI_ENABLED=y
  - CONFIG_ESP_WIFI_SOFTAP_SUPPORT=y
  - CONFIG_ESP_WIFI_NVS_ENABLED=y
- Network interface and TCP/IP stack enabled:
  - CONFIG_ESP_NETIF_TCPIP_LWIP=y
  - CONFIG_ESP_NETIF_USES_TCPIP_WITH_BSD_API=y
- DHCP server support for SoftAP clients:
  - CONFIG_LWIP_DHCPS=y
  - CONFIG_LWIP_DHCPS_MAX_STATION_NUM=8
