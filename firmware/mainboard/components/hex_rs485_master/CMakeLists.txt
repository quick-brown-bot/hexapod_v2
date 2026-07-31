idf_component_register(
    SRCS "rs485_master.c"
    INCLUDE_DIRS "."
    REQUIRES
    esp_driver_uart
    esp_driver_gpio
    esp_timer
    hex_robot_config
)
