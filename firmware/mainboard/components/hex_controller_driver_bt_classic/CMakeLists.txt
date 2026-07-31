idf_component_register(
    SRCS "controller_bt_classic.c"
    INCLUDE_DIRS "."
    REQUIRES bt nvs_flash hex_rpc_transport hex_controller_core hex_shared_types
)
