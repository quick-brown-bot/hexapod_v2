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

def test_controller_namespace_appears_in_namespace_listing(send_rpc):
    response = send_rpc("list namespaces")
    _assert_rpc_success(response, "list namespaces")
    assert "controller" in response, f"Expected 'controller' namespace in response, got: {response!r}"

def test_controller_list_includes_core_parameters(send_rpc):
    response = send_rpc("list controller")
    _assert_rpc_success(response, "list controller")
    assert "list controller:" in response, f"Expected controller listing prefix, got: {response!r}"
    # Only test parameters that are present in the list output and respond to get/set
    expected_params = [
        "driver_type",
        "task_stack",
        "task_priority",
        "stick_deadband",
        "failsafe_vx",
        "failsafe_wz",
        "failsafe_z_target",
        "failsafe_y_offset",
        "failsafe_step_scale",
        "failsafe_gait",
    ]
    for param in expected_params:
        assert param in response, (
            f"Expected controller parameter '{param}' in list output, got: {response!r}"
        )

@pytest.mark.parametrize(
    "param_name,pattern",
    [
        ("driver_type", r"^\d+$"),
        ("task_stack", r"^\d+$"),
        ("stick_deadband", r"^-?\d+\.\d{2,}$"),
        ("failsafe_gait", r"^\d+$"),
    ],
)
def test_controller_get_returns_expected_format(send_rpc, param_name, pattern):
    command = f"get controller {param_name}"
    response = send_rpc(command)
    _assert_rpc_success(response, command)
    value = _parse_kv(response, param_name)
    assert re.match(pattern, value), (
        f"Expected {param_name} to match /{pattern}/, got value {value!r} from {response!r}"
    )


# Test set/get for a parameter that is present and responds (e.g., stick_deadband)
def test_controller_set_mem_updates_value_and_restores(send_rpc):
    get_cmd = "get controller stick_deadband"
    original_response = send_rpc(get_cmd)
    _assert_rpc_success(original_response, get_cmd)
    original_value = _parse_kv(original_response, "stick_deadband")
    # Accept float string
    try:
        new_value = "0.05" if original_value != "0.05" else "0.04"
        set_cmd = f"set controller stick_deadband {new_value}"
        set_response = send_rpc(set_cmd)
        _assert_rpc_success(set_response, set_cmd)
        assert f"stick_deadband={new_value}" in set_response, (
            f"Expected set echo in response, got: {set_response!r}"
        )
        assert "(mem)" in set_response, f"Expected mem set marker, got: {set_response!r}"

        verify_response = send_rpc(get_cmd)
        _assert_rpc_success(verify_response, get_cmd)
        readback = float(_parse_kv(verify_response, "stick_deadband"))
        assert readback == pytest.approx(float(new_value), abs=1e-6), (
            f"Expected stick_deadband approximately {new_value}, got: {verify_response!r}"
        )
    finally:
        restore_cmd = f"set controller stick_deadband {original_value}"
        restore_response = send_rpc(restore_cmd)
        _assert_rpc_success(restore_response, restore_cmd)

@pytest.mark.parametrize(
    "param_name,invalid_value",
    [
        ("task_stack", "100"),
        ("stick_deadband", "0.5"),
        ("failsafe_gait", "5"),
    ],
)
def test_controller_set_rejects_out_of_range_values(send_rpc, param_name, invalid_value):
    command = f"set controller {param_name} {invalid_value}"
    response = send_rpc(command)
    assert response.strip(), f"Expected non-empty response for command: {command}"
    assert "ERROR set" in response, (
        f"Expected out-of-range rejection for {param_name}={invalid_value}, got: {response!r}"
    )

def test_controller_setpersist_persists_and_can_be_restored(send_rpc):
    param_name = "task_priority"
    get_cmd = f"get controller {param_name}"

    original_response = send_rpc(get_cmd)
    _assert_rpc_success(original_response, get_cmd)
    original_value = int(_parse_kv(original_response, param_name))

    candidate_values = [10, 11, 12, 15]
    new_value = next((value for value in candidate_values if value != original_value), None)
    assert new_value is not None, "Failed to find alternate task_priority test value"

    setpersist_cmd = f"setpersist controller {param_name} {new_value}"

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
        restore_cmd = f"setpersist controller {param_name} {original_value}"
        restore_response = send_rpc(restore_cmd)
        _assert_rpc_success(restore_response, restore_cmd)