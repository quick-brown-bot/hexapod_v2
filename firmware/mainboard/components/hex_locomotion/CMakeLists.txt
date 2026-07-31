idf_component_register(
    SRCS
    "user_command.c"
    "gait_scheduler.c"
    "swing_trajectory.c"
    "whole_body_control.c"
    INCLUDE_DIRS "."
    REQUIRES
    hex_controller_core
    hex_controller_driver_flysky_ibus
    hex_controller_driver_wifi_tcp
    hex_controller_driver_bt_classic
    hex_config_manager
    hex_robot_config
    hex_kinematics
)
