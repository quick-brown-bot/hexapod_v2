---
name: config-manager-namespace-checklist
description: 'Use when adding or refactoring a hex_config_manager namespace such as leg_geom, motion_lim, servo_map, joint_cal, gait, or system. Enforces a complete checklist across enum wiring, namespace folder structure, descriptor callbacks, defaults, NVS persistence, parameter discovery, build registration, runtime consumption, fail-fast config loading, and verification.'
argument-hint: 'Describe the namespace name, target runtime subsystem, and whether this is a new namespace or a refactor.'
---

# Config Manager Namespace Checklist

Use this skill when creating a new `hex_config_manager` namespace or refactoring an existing one so it fully participates in descriptor registration, persistence, runtime access, and subsystem consumption.

## When to Use

- Add a new namespace like `leg_geom`, `motion_lim`, `servo_map`, `joint_cal`, `gait`, or `system`.
- Refactor an existing namespace into the catalog/descriptor pattern.
- Check whether a namespace integration is complete, not just compilable.
- Review whether a runtime subsystem is still bypassing config manager with compile-time constants or local defaults.

## Inputs to Establish First

- Namespace string and intended `config_namespace_t` enum entry.
- Namespace-owned runtime struct types and public API surface.
- Which runtime subsystem consumes the namespace.
- Whether the integration is strict: required at startup, or optional.

## Naming Rules

- Use short, console-friendly namespace strings such as `system`, `joint_cal`, `leg_geom`, or `motion_lim`.
- Keep namespace strings stable across descriptor code, RPC flows, console commands, and docs.
- Prefer concise parameter names that are easy to type and inspect.

## Procedure

1. Define namespace identity and ownership.
   - Add the new enum entry in `config_namespace_t` in `config_manager_core_types.h` before `CONFIG_NS_COUNT`.
   - Add or update namespace-specific config struct types in namespace API headers, not in manager core headers.
   - Keep manager-facing access generic through `config_manager_runtime_api.h` and `config_manager_param_api.h`.

2. Create the namespace folder contract.
   - Create `components/hex_config_manager/namespaces/<namespace>/`.
   - Keep namespace-owned implementation inside that folder.
   - Minimum expected files:
     - `config_ns_<namespace>.h`
     - `config_ns_<namespace>.c`
     - `config_ns_<namespace>_api.h` when public types or accessors are needed
     - defaults, persistence, or accessor files as needed
   - Do not move namespace-specific logic into root-level manager files.

3. Implement the descriptor module.
   - Create a dedicated descriptor header and source pair for the namespace.
   - Define a namespace context struct in the header with:
     - `nvs_handle_t*`
     - `namespace_dirty` pointer
     - `namespace_loaded` pointer
     - pointer to the namespace cache struct
   - Export `const config_namespace_descriptor_t g_<namespace>_namespace_descriptor`.
   - Implement callbacks for:
     - `load_defaults`
     - `load_from_nvs`
     - `write_defaults_to_nvs`
     - `save`
     - typed get and set handlers relevant to the namespace
     - parameter listing and parameter info handlers

4. Register the descriptor in the namespace catalog.
   - Add the descriptor header include in `config_namespace_catalog.c`.
   - Append the descriptor and context entry in catalog registration order.
   - Ensure registration still flows through `config_namespace_registry_register(descriptor, context)`.
   - Do not wire namespace globals directly in `config_manager.c`.

5. Implement defaults in one place.
   - Keep namespace defaults in namespace-local files under `namespaces/<namespace>/`.
   - Align defaults with real runtime behavior already expected elsewhere in the project.
   - Do not add hidden consumer-side fallback defaults.

6. Implement NVS persistence.
   - Keep write-defaults, load, and save behavior in namespace-local persistence code.
   - Use compact, consistent NVS keys.
   - Commit writes with `nvs_commit` in save and default-initialization paths.

7. Implement parameter discovery and metadata.
   - If the namespace is parameterized or typed, support parameter list generation, parsing, and metadata reporting.
   - Ensure `config_list_parameters` works for the namespace.
   - Ensure `config_get_parameter_info` works for the namespace.
   - Enforce validation ranges and constraints in set handlers.

8. Wire the build.
   - Add all new namespace source files to `components/hex_config_manager/CMakeLists.txt` under the matching `namespaces/<namespace>/` paths.
   - Ensure include directories expose public headers when external consumers need them.
   - Verify the namespace sources appear in build output.

9. Validate runtime listing and save paths.
   - Confirm `list namespaces` shows the namespace.
   - Confirm `save <namespace>` works through `config_manager_save_namespace_by_name`.
   - Ensure `config_list_namespaces` indexes by `ns_id`, not registry iteration index.
   - Verify the target runtime subsystem actually reads from the namespace at runtime.
   - If the subsystem still uses compile-time constants or local defaults, finish that integration before treating the namespace as complete.
   - If the namespace does not appear at runtime, verify flashed firmware is current.

10. Enforce the runtime configuration contract.
   - Treat namespace-backed configuration as the single source of truth when the subsystem is meant to be runtime-configurable.
   - Fail fast with clear logs when required namespace data is missing, unloaded, or invalid.
   - Validate initialization order so config manager loads before consuming subsystems.
   - Log the effective configuration source at startup and prefer namespace-backed configuration over fallback behavior.

11. Sync documentation.
   - Start from `../../docs/configuration/CONFIG_MANAGER_NAMESPACE_TEMPLATE.md` when documenting a namespace.
   - Keep design docs, examples, and RPC samples aligned to the real namespace string and parameter names.
   - Keep folder-path examples aligned with `namespaces/<namespace>/`.

12. Run the verification checklist.
   - Build succeeds with no new errors.
   - `list namespaces` includes the new namespace.
   - `list <namespace>` returns expected parameter names.
   - `get` and `set` work for at least one parameter.
   - `setpersist` and `save <namespace>` persist and survive reboot.
   - Runtime subsystem behavior changes when namespace values change.
   - Strict integrations fail clearly, or block subsystem startup, when required namespace config is unavailable.
   - Adding the namespace required only:
     - new files in one namespace folder
     - one registration edit in `config_namespace_catalog.c`
     - optional docs or tests updates

## Decision Points

- New namespace or refactor:
  - New namespace: create the full folder, descriptor, defaults, persistence, and build wiring.
  - Refactor: preserve namespace string stability and migrate existing consumers before removing old paths.
- Public API or private-only:
  - Public API needed: add `config_ns_<namespace>_api.h` with owned types and accessors.
  - Private-only namespace: keep headers minimal and internal.
- Strict runtime dependency or optional feature:
  - Strict: fail fast if namespace load or validation fails.
  - Optional: degraded behavior is acceptable only when explicitly designed and clearly logged.
- Generic parameter model or fixed fields:
  - Generic or typed namespace: implement parameter listing, parsing, and metadata.
  - Fixed-only namespace: still support discovery and validation where runtime tooling expects it.

## Completion Criteria

- The namespace is discoverable through runtime listing APIs and console flows.
- Persistence works end to end through NVS and survives reboot.
- The consuming subsystem uses namespace-backed values at runtime.
- Defaults live in the namespace layer, not duplicated in consumers.
- Missing required config fails loudly instead of silently falling back.
- The implementation remains modular: namespace folder plus one catalog registration change.

## Review Focus

Pay extra attention to these common failure modes:

- Enum added but descriptor not registered in the catalog.
- Namespace exists in config manager but is not consumed by the runtime subsystem.
- Parameter listing or metadata handlers are missing, so tooling support is incomplete.
- Consumer code silently reintroduces local fallback defaults.
- Build wiring omits one namespace source file.
- Runtime validation passes only because stale firmware is still flashed.

## Example Prompts

- Add a new `motion_lim` namespace using the config-manager-namespace-checklist skill.
- Review whether `servo_map` is fully integrated or only registered.
- Refactor `leg_geom` to follow the namespace catalog and fail-fast runtime contract.
- Validate that `joint_cal` persists, lists parameters, and is consumed by actuation at runtime.