# Servo Map Namespace

## Purpose
This namespace owns per-leg servo hardware mapping for runtime actuation:
- Servo GPIO per leg/joint (`coxa`, `femur`, `tibia`)
- MCPWM group per leg
- Servo driver selection per leg/joint (`0 = MCPWM`, `1 = LEDC`)

Defaults are defined once in `config_domain_servo_map_defaults.c`, persisted in NVS, and consumed by `hex_robot_config` during startup.

## SDKConfig Requirements
No additional namespace-specific `sdkconfig` options are required.

Runtime depends on platform driver support already provided by ESP-IDF:
- MCPWM support (`CONFIG_SOC_MCPWM_GROUPS`)
- LEDC support (`CONFIG_SOC_LEDC_SUPPORTED`)
