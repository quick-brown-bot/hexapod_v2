idf_component_register(
    SRCS "controller_flysky_ibus.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_uart esp_driver_gpio hex_controller_core hex_shared_types
)
