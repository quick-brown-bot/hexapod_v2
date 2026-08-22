# Hexapod Agent Guide

This file helps coding agents become productive quickly in this repository.

## Scope and priorities

- Use this repository's existing instruction and skill files before inventing new patterns.
- Keep edits minimal and local; avoid broad refactors unless asked.
- Do not edit generated ESP-IDF output under `build/`.

## Start here

- Project overview: [README.md](README.md)
- Docs hub: [../../docs/README.md](../../docs/README.md)
- System architecture: [../../docs/architecture/SYSTEM_ARCHITECTURE.md](../../docs/architecture/SYSTEM_ARCHITECTURE.md)
- Development setup: [../../docs/development/README.md](../../docs/development/README.md)
- Configuration persistence and namespaces: [../../docs/configuration/CONFIGURATION_PERSISTENCE_DESIGN.md](../../docs/configuration/CONFIGURATION_PERSISTENCE_DESIGN.md)

## Build, flash, and test

Run from repository root in an ESP-IDF-enabled environment:

```bash
idf.py build
idf.py flash
idf.py monitor
```

Host-side integration tests:

```bash
pip install -r test/requirements.txt
python -m pytest -q -p no:embedded test/test_config_general_listing.py
```

Test environment notes:

- Host tests target Windows + live robot over Wi-Fi.
- Optional overrides: `HEXAPOD_IP` and `HEXAPOD_PORT`.

## High-value repo conventions

- Runtime-tunable values must be namespace-backed in `hex_config_manager`.
- Defaults are single-source in namespace defaults (no consumer-local fallback defaults).
- Required config should fail fast when missing, unloaded, or invalid.
- Keep persistence/migration/validation ownership in `hex_config_manager`, not in consuming components.

For full rules, follow [config-runtime-contract.md](../../.claude/rules/mainboard/config-runtime-contract.md).

## Use these rules

Path-scoped rules under `.claude/rules/` load automatically when Claude reads a matching file; the docs philosophy rule also applies repo-wide.

- Components: [component-readme-requirements.md](../../.claude/rules/mainboard/component-readme-requirements.md)
- Runtime config contract: [config-runtime-contract.md](../../.claude/rules/mainboard/config-runtime-contract.md)
- Documentation structure (repo-wide): [documentation-structure-and-readme-philosophy.md](../../.claude/rules/documentation-structure-and-readme-philosophy.md)
- Python integration tests: [integration-testing-principles.md](../../.claude/rules/mainboard/integration-testing-principles.md)

## Use these skills

- Namespace checklist: [config-manager-namespace-checklist](.claude/skills/config-manager-namespace-checklist/SKILL.md)
- Live namespace pytest workflow: [config-manager-pytest-integration-tests](.claude/skills/config-manager-pytest-integration-tests/SKILL.md)
- ESP-IDF test layer selection (Unity vs pytest-embedded): [esp-idf-testing-strategy agent](../../.claude/agents/esp-idf-testing-strategy.md)

## Component map

- Bootstrapping: `main/main.c`
- Core runtime components: `components/*`
- RPC path: `components/hex_rpc_core` and `components/hex_rpc_transport`
- Runtime config: `components/hex_config_manager`
- Locomotion pipeline: `components/hex_locomotion`, `components/hex_motion_limits`, `components/hex_actuation`, `components/hex_kinematics`

Prefer component-local README files when editing a component under `components/`.