"""Integration tests for the `joint`/`ik` RS485 bring-up/diagnostic RPC
commands (hex_rpc_core/rpc_commands.c).

Deliberately does NOT test commanding a nonzero joint angle or IK target --
both physically move a real servo, and there is no human here to confirm the
result is safe (see docs/architecture/HARDWARE_AND_MECHANICS.md "Joint Angle
Sign Convention" and docs/development/LEG_CALIBRATION.md for why that
confirmation step matters). Covers argument validation, error paths, and the
idempotent `release` path, which never moves anything.
"""

import re

import pytest


def _assert_rpc_success(response: str, command: str) -> None:
    assert response.strip(), f"Empty RPC response for command: {command}"
    assert "ERROR" not in response, (
        f"Expected success for command '{command}', got: {response!r}"
    )


def test_help_lists_joint_and_ik_commands(send_rpc):
    response = send_rpc("help")
    _assert_rpc_success(response, "help")
    assert "joint" in response, f"Expected 'joint' in help output, got: {response!r}"
    assert "ik" in response, f"Expected 'ik' in help output, got: {response!r}"


@pytest.mark.parametrize("leg", [1, 6])
def test_joint_release_is_safe_and_idempotent(send_rpc, leg):
    # release never moves anything (it's the "hand back to gait/IK" path), so
    # it's safe to run against a live robot with no setup/teardown needed --
    # and it's idempotent, so running it twice in a row must behave the same.
    command = f"joint {leg} release"
    first = send_rpc(command)
    second = send_rpc(command)
    for response in (first, second):
        _assert_rpc_success(response, command)
        assert "released" in response, f"Expected release confirmation, got: {response!r}"
        assert str(leg) in response, f"Expected leg number echoed, got: {response!r}"


@pytest.mark.parametrize("leg", [0, 7, -1])
def test_joint_rejects_out_of_range_leg(send_rpc, leg):
    command = f"joint {leg} release"
    response = send_rpc(command)
    assert "out of range" in response, (
        f"Expected out-of-range error for leg {leg}, got: {response!r}"
    )


def test_joint_reports_usage_on_missing_args(send_rpc):
    response = send_rpc("joint 1")
    assert "usage" in response.lower(), (
        f"Expected a usage message for a bare 'joint <leg>', got: {response!r}"
    )


@pytest.mark.parametrize("leg", [0, 7, -1])
def test_ik_rejects_out_of_range_leg(send_rpc, leg):
    command = f"ik {leg} 0.1 0 -0.1"
    response = send_rpc(command)
    assert "out of range" in response, (
        f"Expected out-of-range error for leg {leg}, got: {response!r}"
    )


def test_ik_reports_usage_on_missing_args(send_rpc):
    response = send_rpc("ik 1")
    assert "usage" in response.lower(), (
        f"Expected a usage message for a bare 'ik <leg>', got: {response!r}"
    )


def test_unknown_top_level_command_is_rejected(send_rpc):
    # Sanity check that `joint`/`ik` didn't accidentally swallow the
    # dispatcher's fallback -- an unrelated bogus command must still hit it.
    response = send_rpc("not-a-real-command")
    assert re.search(r"unknown", response, re.IGNORECASE), (
        f"Expected an 'unknown command' style response, got: {response!r}"
    )
