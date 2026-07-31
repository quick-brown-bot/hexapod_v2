idf_component_register(
    SRCS "controller_wifi_tcp.c"
    INCLUDE_DIRS "."
    REQUIRES esp_netif lwip hex_rpc_transport hex_wifi_ap hex_controller_core hex_shared_types
)
