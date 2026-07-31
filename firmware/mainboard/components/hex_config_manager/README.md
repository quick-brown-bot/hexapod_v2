# hex_config_manager

## Role
Configuration domain management, registry dispatch, migration, and NVS persistence for robot/system settings.

## Responsibilities
- Own all namespace descriptors and parameter registries.
- Provide typed read/write APIs for system and joint configuration values.
- Load defaults, load persisted data, and run schema migrations during startup.
- Persist namespace data to NVS, including on-demand namespace save by name.

## Public Surface
- Headers:
  - config_manager_core_types.h
  - config_manager_runtime_api.h
  - config_manager_param_api.h
  - namespaces/*/config_ns_*_api.h
- Primary APIs:
  - config_manager_init
  - config_manager_get_state
  - config_manager_save_namespace
  - config_manager_save_namespace_by_name
  - config_list_namespaces
  - config_list_parameters
  - config_get_parameter_info
  - hexapod_config_get_*
  - hexapod_config_set_*

## Internal Modules
- config_namespace_registry: runtime namespace descriptor registry.
- namespaces/system/*: system namespace descriptor, access, defaults, persistence.
- namespaces/joint_cal/*: joint_cal namespace descriptor, access, defaults, persistence, metadata registry.
- namespaces/leg_geom/*: leg_geom namespace descriptor, defaults, persistence.
- namespaces/motion_lim/*: motion_lim namespace descriptor, defaults, persistence.
- namespaces/controller/*: controller namespace descriptor, access, defaults, persistence.
- namespaces/wifi/*: wifi namespace descriptor, access, defaults, persistence.
- config_domain_defaults: default value seeding helpers.
- config_migration: schema migration coordination.
- config_storage_nvs: low-level NVS read/write helpers.

## Integration
- Application startup initializes this component before controller and motion loops.
- RPC command handling queries and mutates values through this component APIs.
- Robot calibration/bootstrap code reads configuration through typed accessors.

## SDKConfig Requirements (Current Project)
- NVS support must be enabled in ESP-IDF (default project setting):
  - CONFIG_NVS_FLASH=y
- NVS partition must exist in partition table used by this project.
- This component assumes logging support is enabled:
  - CONFIG_LOG_DEFAULT_LEVEL (project-defined level)
