idf_component_register(
    SRCS "rpc_commands.c"
    INCLUDE_DIRS "."
    REQUIRES
    hex_rpc_transport
    hex_config_manager
    hex_controller_core
    hex_shared_types
)
