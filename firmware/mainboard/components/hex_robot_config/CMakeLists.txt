idf_component_register(
    SRCS "robot_config.c"
    INCLUDE_DIRS "."
    REQUIRES
    hex_kinematics
    hex_config_manager
    hex_shared_types
)
