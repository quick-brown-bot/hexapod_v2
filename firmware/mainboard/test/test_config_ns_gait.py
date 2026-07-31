import re
import pytest

# Helper functions (mirrored from system/controller tests)
def _assert_rpc_success(response: str, command: str) -> None:
    assert response.strip(), f"Empty RPC response for command: {command}"
    assert "ERROR" not in response, (
        f"Expected success for command '{command}', got: {response!r}"
    )

def _parse_kv(response: str, expected_param: str) -> str:
    match = re.search(rf"{re.escape(expected_param)}=(.+)", response)
    assert match, f"Expected '{expected_param}=<value>' in response, got: {response!r}"
    return match.group(1).strip()

PARAM_SUFFIXES = [
    "cycle_time_s", "step_length_m", "clearance_height_m", "y_range_m",
    "z_min_m", "z_max_m", "max_yaw_per_cycle_rad", "turn_direction"
]

def test_gait_namespace_appears_in_namespace_listing(send_rpc):
    response = send_rpc("list namespaces")
    _assert_rpc_success(response, "list namespaces")
    assert "gait" in response, f"Expected 'gait' namespace in response, got: {response!r}"

def test_gait_list_includes_core_parameters(send_rpc):
    response = send_rpc("list gait")
    _assert_rpc_success(response, "list gait")
    assert "list gait:" in response, f"Expected gait listing prefix, got: {response!r}"
    # Test a subset due to firmware limitations (only first few params returned)
    expected_params = [
        "cycle_time_s", "step_length_m", "clearance_height_m", "y_range_m"
    ]
    for param in expected_params:
        assert param in response, (
            f"Expected gait parameter '{param}' in list output, got: {response!r}"
        )

@pytest.mark.parametrize("param_suffix", PARAM_SUFFIXES)
def test_gait_get_returns_float(send_rpc, param_suffix):
    param_name = param_suffix
    command = f"get gait {param_name}"
    response = send_rpc(command)
    _assert_rpc_success(response, command)
    value = _parse_kv(response, param_name)
    # Accept floats like 0.068, -0.1, 1.0, etc.
    assert re.match(r"^-?\d+\.\d+$", value), (
        f"Expected {param_name} to match float pattern, got value {value!r} from {response!r}"
    )

# Test set/get for a parameter (e.g., cycle_time_s)
def test_gait_set_mem_updates_value_and_restores(send_rpc):
    param_name = "cycle_time_s"
    get_cmd = f"get gait {param_name}"
    original_response = send_rpc(get_cmd)
    _assert_rpc_success(original_response, get_cmd)
    original_value = _parse_kv(original_response, param_name)
    try:
        new_value = "1.6" if original_value != "1.6" else "1.7"
        set_cmd = f"set gait {param_name} {new_value}"
        set_response = send_rpc(set_cmd)
        _assert_rpc_success(set_response, set_cmd)
        assert f"{param_name}={new_value}" in set_response, (
            f"Expected set echo in response, got: {set_response!r}"
        )
        assert "(mem)" in set_response, f"Expected mem set marker, got: {set_response!r}"
        verify_response = send_rpc(get_cmd)
        _assert_rpc_success(verify_response, get_cmd)
        readback = float(_parse_kv(verify_response, param_name))
        assert readback == pytest.approx(float(new_value), abs=1e-6), (
            f"Expected {param_name} approximately {new_value}, got: {verify_response!r}"
        )
    finally:
        restore_cmd = f"set gait {param_name} {original_value}"
        restore_response = send_rpc(restore_cmd)
        _assert_rpc_success(restore_response, restore_cmd)

@pytest.mark.parametrize(
    "param_name,invalid_value",
    [
        ("cycle_time_s", "0.05"),
        ("cycle_time_s", "15.0"),
        ("step_length_m", "-0.1"),
        ("step_length_m", "1.0"),
        ("turn_direction", "-2.0"),
        ("turn_direction", "2.0"),
    ],
)
def test_gait_set_rejects_out_of_range_values(send_rpc, param_name, invalid_value):
    command = f"set gait {param_name} {invalid_value}"
    response = send_rpc(command)
    assert response.strip(), f"Expected non-empty response for command: {command}"
    assert "ERROR set" in response, (
        f"Expected out-of-range rejection for {param_name}={invalid_value}, got: {response!r}"
    )

def test_gait_setpersist_persists_and_can_be_restored(send_rpc):
    param_name = "step_length_m"
    get_cmd = f"get gait {param_name}"
    original_response = send_rpc(get_cmd)
    _assert_rpc_success(original_response, get_cmd)
    original_value = float(_parse_kv(original_response, param_name))
    candidate_values = [0.08, 0.09, 0.10]
    new_value = next((v for v in candidate_values if abs(v - original_value) > 1e-6), None)
    assert new_value is not None, "Failed to find alternate test value"
    setpersist_cmd = f"setpersist gait {param_name} {new_value}"
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
        assert float(_parse_kv(verify_response, param_name)) == pytest.approx(new_value, abs=1e-6), (
            f"Expected {param_name} to read back {new_value}, got: {verify_response!r}"
        )
    finally:
        restore_cmd = f"setpersist gait {param_name} {original_value}"
        restore_response = send_rpc(restore_cmd)
        _assert_rpc_success(restore_response, restore_cmd)