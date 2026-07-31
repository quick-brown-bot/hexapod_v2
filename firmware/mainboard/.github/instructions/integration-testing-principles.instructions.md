---
name: "Integration Testing Principles"
description: "Use when writing or refactoring Python integration tests, RPC tests, transport-level tests, or shared pytest fixtures. Enforces externally visible interfaces, deterministic behavior, fixture-based bootstrap, configuration cleanup, and parameterized coverage."
applyTo:
  - "test/**/*.py"
  - "pytest_*.py"
---

# Integration Testing Principles

- Prefer externally visible interfaces over direct internal access. Drive behavior through RPC commands, network transports, or other public integration surfaces instead of reaching into implementation details.
- Keep integration tests deterministic and repeatable. Use explicit inputs, bounded timeouts, stable assertions, and clear skip conditions for missing external prerequisites.
- Avoid hidden ordering dependencies. Each test must set up its own required state and must not rely on another test having run first.
- Restore mutated runtime configuration when practical. If a test changes persisted or in-memory configuration, capture the original value first and restore it in teardown or `finally` cleanup.
- Put shared bootstrap and transport setup in fixtures or helpers. Connection setup, device discovery, and common RPC helpers belong in `conftest.py` or reusable helper functions, not repeated inline across test files.
- Prefer parameterized tests over copy-pasted cases when the same integration contract is validated across many parameters or commands.

## Review Checklist

- The test exercises a public integration boundary.
- The test can run independently and repeatedly with the same expected result.
- Any mutated runtime state is restored or explicitly isolated.
- Shared setup logic is factored into fixtures or helpers.
- Repeated cases use `pytest.mark.parametrize` instead of cloned test bodies.