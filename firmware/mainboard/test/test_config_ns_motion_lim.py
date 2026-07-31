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
    "max_velocity_coxa", "max_velocity_femur", "max_velocity_tibia",
    "max_acceleration_coxa", "max_acceleration_femur", "max_acceleration_tibia",
    "max_jerk_coxa", "max_jerk_femur", "max_jerk_tibia",
    "velocity_filter_alpha", "accel_filter_alpha", "leg_velocity_filter_alpha",
    "body_velocity_filter_alpha", "body_pitch_filter_alpha", "body_roll_filter_alpha",
    "front_to_back_distance", "left_to_right_distance", "max_leg_velocity",
    "max_body_velocity", "max_angular_velocity", "min_dt", "max_dt",
    "body_offset_x", "body_offset_y", "body_offset_z"
]

def test_motion_lim_namespace_appears_in_namespace_listing(send_rpc):
    response = send_rpc("list namespaces")
    _assert_rpc_success(response, "list namespaces")
    assert "motion_lim" in response, f"Expected 'motion_lim' namespace in response, got: {response!r}"

def test_motion_lim_list_includes_core_parameters(send_rpc):
    response = send_rpc("list motion_lim")
    _assert_rpc_success(response, "list motion_lim")
    assert "list motion_lim:" in response, f"Expected motion_lim listing prefix, got: {response!r}"
    # Test a subset due to firmware limitations (only first 10 params returned)
    expected_params = [
        "max_velocity_coxa", "max_velocity_femur", "max_velocity_tibia",
        "max_acceleration_coxa", "max_acceleration_femur", "max_acceleration_tibia",
        "max_jerk_coxa", "max_jerk_femur", "max_jerk_tibia", "velocity_filter_alpha"
    ]
    for param in expected_params:
        assert param in response, (
            f"Expected motion_lim parameter '{param}' in list output, got: {response!r}"
        )

@pytest.mark.parametrize("param_suffix", PARAM_SUFFIXES)
def test_motion_lim_get_returns_float(send_rpc, param_suffix):
    param_name = param_suffix
    command = f"get motion_lim {param_name}"
    response = send_rpc(command)
    _assert_rpc_success(response, command)
    value = _parse_kv(response, param_name)
    # Accept floats like 0.068, -0.1, 1.0, etc.
    assert re.match(r"^-?\d+\.\d+$", value), (
        f"Expected {param_name} to match float pattern, got value {value!r} from {response!r}"
    )

# Test set/get for a parameter (e.g., max_velocity_coxa)
def test_motion_lim_set_mem_updates_value_and_restores(send_rpc):
    param_name = "max_velocity_coxa"
    get_cmd = f"get motion_lim {param_name}"
    original_response = send_rpc(get_cmd)
    _assert_rpc_success(original_response, get_cmd)
    original_value = _parse_kv(original_response, param_name)
    try:
        new_value = "6.0" if original_value != "6.0" else "7.0"
        set_cmd = f"set motion_lim {param_name} {new_value}"
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
        restore_cmd = f"set motion_lim {param_name} {original_value}"
        restore_response = send_rpc(restore_cmd)
        _assert_rpc_success(restore_response, restore_cmd)

@pytest.mark.parametrize(
    "param_name,invalid_value",
    [
        ("max_velocity_coxa", "-1.0"),
        ("max_velocity_coxa", "25.0"),
        ("velocity_filter_alpha", "-0.1"),
        ("velocity_filter_alpha", "1.1"),
        ("front_to_back_distance", "-0.1"),
    ],
)
def test_motion_lim_set_rejects_out_of_range_values(send_rpc, param_name, invalid_value):
    command = f"set motion_lim {param_name} {invalid_value}"
    response = send_rpc(command)
    assert response.strip(), f"Expected non-empty response for command: {command}"
    assert "ERROR set" in response, (
        f"Expected out-of-range rejection for {param_name}={invalid_value}, got: {response!r}"
    )

def test_motion_lim_setpersist_persists_and_can_be_restored(send_rpc):
    param_name = "velocity_filter_alpha"
    get_cmd = f"get motion_lim {param_name}"
    original_response = send_rpc(get_cmd)
    _assert_rpc_success(original_response, get_cmd)
    original_value = float(_parse_kv(original_response, param_name))
    candidate_values = [0.4, 0.5, 0.6, 0.7]
    new_value = next((v for v in candidate_values if abs(v - original_value) > 1e-6), None)
    assert new_value is not None, "Failed to find alternate test value"
    setpersist_cmd = f"setpersist motion_lim {param_name} {new_value}"
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
        restore_cmd = f"setpersist motion_lim {param_name} {original_value}"
        restore_response = send_rpc(restore_cmd)
        _assert_rpc_success(restore_response, restore_cmd)