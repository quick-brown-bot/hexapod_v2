import re

import pytest


def _assert_rpc_success(response: str, command: str) -> None:
    assert response.strip(), f"Empty RPC response for command: {command}"
    assert "ERROR" not in response, (
        f"Expected success for command '{command}', got: {response!r}"
    )


def _parse_kv(response: str, expected_param: str) -> str:
    match = re.search(rf"{re.escape(expected_param)}=(.+)", response)
    assert match, f"Expected '{expected_param}=<value>' in response, got: {response!r}"
    return match.group(1).strip()


def test_system_namespace_appears_in_namespace_listing(send_rpc):
    response = send_rpc("list namespaces")
    _assert_rpc_success(response, "list namespaces")
    assert "system" in response, f"Expected 'system' namespace in response, got: {response!r}"


def test_system_list_includes_core_parameters(send_rpc):
    response = send_rpc("list system")
    _assert_rpc_success(response, "list system")
    assert "list system:" in response, f"Expected system listing prefix, got: {response!r}"

    expected_params = [
        "emergency_stop_enabled",
        "auto_disarm_timeout",
        "safety_voltage_min",
        "temperature_limit_max",
        "robot_id",
    ]
    for param in expected_params:
        assert param in response, (
            f"Expected system parameter '{param}' in list output, got: {response!r}"
        )


@pytest.mark.parametrize(
    "param_name,pattern",
    [
        ("emergency_stop_enabled", r"^(true|false)$"),
        ("auto_disarm_timeout", r"^\d+$"),
        ("safety_voltage_min", r"^\d+\.\d{3}$"),
        ("robot_id", r"^\S+$"),
    ],
)
def test_system_get_returns_expected_format(send_rpc, param_name, pattern):
    command = f"get system {param_name}"
    response = send_rpc(command)
    _assert_rpc_success(response, command)
    value = _parse_kv(response, param_name)
    assert re.match(pattern, value), (
        f"Expected {param_name} to match /{pattern}/, got value {value!r} from {response!r}"
    )


def test_system_set_mem_updates_value_and_restores(send_rpc):
    get_cmd = "get system emergency_stop_enabled"
    original_response = send_rpc(get_cmd)
    _assert_rpc_success(original_response, get_cmd)
    original_value = _parse_kv(original_response, "emergency_stop_enabled")
    assert original_value in {"true", "false"}, (
        f"Unexpected bool value for emergency_stop_enabled: {original_value!r}"
    )

    new_value = "false" if original_value == "true" else "true"
    set_cmd = f"set system emergency_stop_enabled {new_value}"

    try:
        set_response = send_rpc(set_cmd)
        _assert_rpc_success(set_response, set_cmd)
        assert f"emergency_stop_enabled={new_value}" in set_response, (
            f"Expected set echo in response, got: {set_response!r}"
        )
        assert "(mem)" in set_response, f"Expected mem set marker, got: {set_response!r}"

        verify_response = send_rpc(get_cmd)
        _assert_rpc_success(verify_response, get_cmd)
        assert _parse_kv(verify_response, "emergency_stop_enabled") == new_value, (
            f"Expected emergency_stop_enabled={new_value}, got: {verify_response!r}"
        )
    finally:
        restore_cmd = f"set system emergency_stop_enabled {original_value}"
        restore_response = send_rpc(restore_cmd)
        _assert_rpc_success(restore_response, restore_cmd)


@pytest.mark.parametrize(
    "param_name,invalid_value",
    [
        ("auto_disarm_timeout", "0"),
        ("safety_voltage_min", "2.9"),
        ("temperature_limit_max", "120.0"),
    ],
)
def test_system_set_rejects_out_of_range_values(send_rpc, param_name, invalid_value):
    command = f"set system {param_name} {invalid_value}"
    response = send_rpc(command)
    assert response.strip(), f"Expected non-empty response for command: {command}"
    assert "ERROR set" in response, (
        f"Expected out-of-range rejection for {param_name}={invalid_value}, got: {response!r}"
    )


def test_system_setpersist_persists_and_can_be_restored(send_rpc):
    param_name = "auto_disarm_timeout"
    get_cmd = f"get system {param_name}"

    original_response = send_rpc(get_cmd)
    _assert_rpc_success(original_response, get_cmd)
    original_value = int(_parse_kv(original_response, param_name))

    candidate_values = [30, 31, 32, 60]
    new_value = next((value for value in candidate_values if value != original_value), None)
    assert new_value is not None, "Failed to find alternate auto_disarm_timeout test value"

    setpersist_cmd = f"setpersist system {param_name} {new_value}"

    try:
        setpersist_response = send_rpc(setpersist_cmd)
        _assert_rpc_success(setpersist_response, setpersist_cmd)
        assert f"{param_name}={new_value}" in setpersist_response, (
            f"Expected setpersist echo in response, got: {setpersist_response!r}"
        )
        assert "(persist)" in setpersist_response, (
            f"Expected persist set marker, got: {setpersist_response!r}"
        )

        verify_response = send_rpc(get_cmd)
        _assert_rpc_success(verify_response, get_cmd)
        assert int(_parse_kv(verify_response, param_name)) == new_value, (
            f"Expected {param_name} to read back {new_value}, got: {verify_response!r}"
        )
    finally:
        restore_cmd = f"setpersist system {param_name} {original_value}"
        restore_response = send_rpc(restore_cmd)
        _assert_rpc_success(restore_response, restore_cmd)


def test_system_factory_reset_restores_factory_defaults_after_save(send_rpc):
    param_name = "auto_disarm_timeout"
    get_cmd = f"get system {param_name}"
    reset_cmd = "factory-reset"

    baseline_reset_response = send_rpc(reset_cmd)
    _assert_rpc_success(baseline_reset_response, reset_cmd)
    assert "factory-reset: OK" in baseline_reset_response, (
        f"Expected factory reset success response, got: {baseline_reset_response!r}"
    )

    baseline_response = send_rpc(get_cmd)
    _assert_rpc_success(baseline_response, get_cmd)
    baseline_value = int(_parse_kv(baseline_response, param_name))

    candidate_values = [31, 32, 60]
    new_value = next((value for value in candidate_values if value != baseline_value), None)
    assert new_value is not None, "Failed to find alternate auto_disarm_timeout test value"

    set_cmd = f"set system {param_name} {new_value}"
    save_cmd = "save system"

    try:
        set_response = send_rpc(set_cmd)
        _assert_rpc_success(set_response, set_cmd)
        assert f"{param_name}={new_value}" in set_response, (
            f"Expected set echo in response, got: {set_response!r}"
        )
        assert "(mem)" in set_response, f"Expected mem set marker, got: {set_response!r}"

        save_response = send_rpc(save_cmd)
        _assert_rpc_success(save_response, save_cmd)
        assert "save system: OK" in save_response, (
            f"Expected save success response, got: {save_response!r}"
        )

        updated_response = send_rpc(get_cmd)
        _assert_rpc_success(updated_response, get_cmd)
        assert int(_parse_kv(updated_response, param_name)) == new_value, (
            f"Expected {param_name} to read back {new_value}, got: {updated_response!r}"
        )

        reset_response = send_rpc(reset_cmd)
        _assert_rpc_success(reset_response, reset_cmd)
        assert "factory-reset: OK" in reset_response, (
            f"Expected factory reset success response, got: {reset_response!r}"
        )

        restored_response = send_rpc(get_cmd)
        _assert_rpc_success(restored_response, get_cmd)
        assert int(_parse_kv(restored_response, param_name)) == baseline_value, (
            f"Expected {param_name} to return to factory value {baseline_value}, got: {restored_response!r}"
        )
    finally:
        final_reset_response = send_rpc(reset_cmd)
        _assert_rpc_success(final_reset_response, reset_cmd)