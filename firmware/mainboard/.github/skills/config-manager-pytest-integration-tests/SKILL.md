---
name: config-manager-pytest-integration-tests
description: 'Use when creating or refactoring pytest integration tests for hex_config_manager namespaces over live robot transport channels. Covers HEXAPOD_xxx Wi-Fi selection, live transport bootstrap, namespace-first test layout, command-level validation, and robot-specific diagnostics.'
argument-hint: 'Describe the namespace under test, transport path, and whether you are adding a new test module or refactoring existing tests.'
---

# Config Manager Pytest Integration Tests

Use this skill when writing Python pytest integration tests that exercise `hex_config_manager` over a live robot transport such as Wi-Fi TCP RPC.

Generic integration-test rules such as determinism, fixture reuse, cleanup, and parameterization are already covered by the workspace instruction set. This skill only adds the robot-specific workflow and structure needed for live namespace testing.

## When to Use

- Add a new namespace integration test module for `hex_config_manager`.
- Refactor existing RPC-based namespace tests to follow the expected live-robot flow.
- Standardize Wi-Fi discovery, connection selection, and live command validation.
- Review whether a namespace test module is organized around real config-manager behaviors.

## Inputs to Establish First

- Namespace under test, for example `system`, `leg_geom`, `motion_lim`, or `servo_map`.
- Live transport path, for example Wi-Fi TCP RPC.
- Host prerequisites such as Windows Wi-Fi control availability and required packages.
- Whether the test should validate listing, get, set, persistence, bounds, or downstream integration impact.

## Procedure

1. Check the current Wi-Fi connection.
   - Inspect the host network state before attempting discovery.
   - Look for an already connected SSID matching `HEXAPOD_xxx`.

2. Reuse a matching robot network when already connected.
   - If the current SSID matches the robot naming pattern, keep it and continue.
   - Record that the existing connection was reused so failures can be triaged later.

3. Discover robot SSIDs when not already connected.
   - Scan available access points.
   - Filter candidates to SSIDs matching the `HEXAPOD_xxx` pattern.

4. Connect to the first matching robot AP.
   - Attempt connection using the agreed bootstrap path.
   - Give the connection a brief settle period before opening the transport socket.

5. Fail clearly when discovery or connection cannot proceed.
   - If no matching SSID is found, skip or fail with a message that explains what discovery returned.
   - If the Wi-Fi control dependency is unavailable or connection fails, surface the reason explicitly.

6. Build the transport fixture layer.
   - Expose a robot-network fixture that resolves SSID, IP, and port.
   - Expose a transport or socket fixture for the live command channel.
   - Expose a small helper for sending a command and capturing the raw response.

7. Organize tests by namespace first.
   - Keep one dedicated module per namespace when validating namespace-local behavior.
   - Use module names like `test_config_ns_<namespace>.py`.
   - Reserve mixed-namespace modules for intentional cross-namespace behaviors only.

8. Structure each namespace module around behavior areas.
   - Listing and discovery: `list namespaces`, `list <namespace>`.
   - Read semantics: `get` response shape and value parsing.
   - Write semantics: `set` success and validation errors.
   - Persistence paths: `setpersist`, `save`, reconnect, and readback.
   - Bounds and constraints: invalid ranges, enum/domain checks.
   - Integration impact: downstream runtime behavior that proves the namespace matters.

9. Validate commands at the transport boundary.
   - Assert the command reached the robot successfully.
   - Assert the returned payload contains the expected namespace, parameter, or status information.
   - On failure, include the command text and raw response in the assertion path.

10. Finish with robot-specific diagnostics.
   - Report whether the connection was reused or newly discovered.
   - Keep failure text specific to the namespace and command under test.
   - Call out stale-flash or wrong-target risks when behavior does not match expected firmware features.

## Decision Points

- Already connected or discover first:
  - Already on `HEXAPOD_xxx`: reuse the current network.
  - Not connected: scan and connect to the first matching SSID.
- Missing prerequisite or missing robot:
  - Missing Wi-Fi control dependency: skip clearly.
  - No matching SSID found: skip or fail clearly based on the test contract.
- Namespace-local or cross-namespace behavior:
  - Namespace-local: keep a dedicated namespace module.
  - Cross-namespace interaction: combine only when the interaction itself is the behavior under test.
- Read-only or state-mutating test slice:
  - Read-only: focus on listing and read semantics.
  - State-mutating: include persistence and restoration flow through the live transport.

## Completion Criteria

- The test module follows the robot Wi-Fi bootstrap flow instead of assuming a fixed connection.
- Transport setup is centralized and reusable.
- Namespace coverage is organized around real config-manager commands.
- Failures identify the SSID choice, command issued, and raw robot response.
- Namespace tests prove more than connectivity by checking actual config-manager behavior.

## Review Focus

Pay extra attention to these robot-specific mistakes:

- Hardcoding one SSID instead of discovering `HEXAPOD_xxx` networks.
- Repeating Wi-Fi or socket bootstrap logic inside individual test bodies.
- Mixing multiple namespaces in one file without an intentional integration reason.
- Treating transport success as sufficient without validating command semantics.
- Hiding connection-selection details that would explain field failures.

## Example Prompts

- Add a live RPC pytest module for the `motion_lim` namespace using the config-manager-pytest-integration-tests skill.
- Refactor `test_config_ns_servo_map.py` to use shared Wi-Fi bootstrap fixtures and namespace-first organization.
- Review whether the `system` namespace tests validate transport responses clearly enough for real robot debugging.