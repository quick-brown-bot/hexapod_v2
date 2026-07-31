idf_component_register(
    SRCS "controller.c"
    INCLUDE_DIRS "."
    REQUIRES freertos hex_shared_types hex_config_manager
)
