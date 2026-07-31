idf_component_register(
    SRCS
    "kpp_system.c"
    "kpp_forward_kin.c"
    INCLUDE_DIRS "."
    REQUIRES
    hex_locomotion
    hex_robot_config
    hex_config_manager
    hex_kinematics
)
