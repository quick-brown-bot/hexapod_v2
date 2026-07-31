---
name: "Config Runtime Contract"
description: "Use when adding, refactoring, or consuming runtime-tunable configuration in components or main startup code. Enforces namespace-backed configuration, single-source defaults, no hidden local fallback defaults, loud failure on missing required config, hex_config_manager ownership of persistence/migration/validation, consumer-side validation before use, shared config contracts, transport-agnostic runtime models, and compatible namespace schema evolution."
applyTo: "components/**, main/**"
---
# Config Runtime Contract

Use these rules whenever a value can be tuned at runtime, persisted, or loaded from configuration.

## 1. Put runtime-tunable values in namespace-backed configuration

- Runtime-tunable values must live in a `hex_config_manager` namespace.
- Do not introduce component-local configurable constants when the value is expected to change at runtime.
- Keep runtime configuration transport-agnostic. Components must not depend on RPC, CLI, or other transport-specific request shapes to define their runtime config model.

## 2. Keep defaults in one place

- Namespace defaults are the single source of truth.
- Define defaults only in the owning namespace defaults layer under `hex_config_manager`.
- Do not duplicate the same default in consuming components, startup code, RPC handlers, or tests.
- Do not add hidden local fallback defaults in components to mask missing namespace-backed data.

## 3. Keep ownership in hex_config_manager

- `hex_config_manager` owns persistence, schema migration, and namespace-level validation for stored configuration.
- Components that consume configuration must not read or write NVS directly for namespace-owned values.
- Components must not duplicate migration logic or persistence key knowledge outside `hex_config_manager`.
- Keep schema ownership in the config domain. Consumers should depend on namespace APIs and shared config-domain types, not redefine storage behavior locally.

## 4. Make consumers strict

- Configuration consumers must validate loaded data before use.
- Required configuration must fail initialization loudly when data is missing, unloaded, or invalid.
- Prefer returning an error, aborting subsystem startup, or logging a clear fatal configuration error over silently continuing with guessed values.
- Wire initialization order so required namespaces are loaded before dependent subsystems start.
- Treat a successful load as necessary but not sufficient. Consumers should still verify semantic invariants, ranges, and structural assumptions before acting on the data.

## 5. Share one contract

- Shared configuration structs, enums, parameter names, and access patterns must not be duplicated across components.
- Define shared contracts once in the owning config namespace API or another shared config-domain header.
- Consumers should use config-manager APIs or shared config-domain types instead of re-declaring equivalent fields locally.

## 6. Evolve schemas compatibly

- Prefer backward-compatible namespace schema changes whenever possible.
- Favor additive changes, explicit migration paths, and stable namespace/parameter names.
- When a breaking change is unavoidable, implement the migration in `hex_config_manager` and keep consumer code free of legacy storage compatibility branches.
- Avoid coupling schema evolution to a specific transport or caller workflow. Namespace evolution should preserve the same runtime contract regardless of whether configuration arrives through RPC, tests, or future interfaces.

## 7. Review checklist

- Ask whether the value is runtime-tunable. If yes, move it into namespace-backed configuration.
- Ask whether the default exists anywhere outside the namespace defaults layer. If yes, remove the duplicate.
- Ask whether the consumer can start with missing config. If no, fail fast with a clear error.
- Ask whether persistence, migration, or schema validation leaked into a consumer component. If yes, move it back into `hex_config_manager`.
- Ask whether the same config contract is declared in more than one place. If yes, consolidate it.
- Ask whether the consumer validated the loaded data before use. If not, add explicit validation at the consumption boundary.
- Ask whether the runtime configuration model assumes a specific transport, request payload, or caller. If yes, move that concern out of the config contract.