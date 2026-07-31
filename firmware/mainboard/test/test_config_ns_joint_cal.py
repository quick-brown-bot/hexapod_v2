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

JOINT_NAMES = ["coxa", "femur", "tibia"]
PARAM_TYPES_FLOAT = ["offset", "min", "max"]
PARAM_TYPES_INT = ["invert", "pwm_min", "pwm_max", "neutral"]

def test_joint_cal_namespace_appears_in_namespace_listing(send_rpc):
    response = send_rpc("list namespaces")
    _assert_rpc_success(response, "list namespaces")
    assert "joint_cal" in response, f"Expected 'joint_cal' namespace in response, got: {response!r}"

def test_joint_cal_list_includes_core_parameters(send_rpc):
    response = send_rpc("list joint_cal")
    _assert_rpc_success(response, "list joint_cal")
    assert "list joint_cal:" in response, f"Expected joint_cal listing prefix, got: {response!r}"
    # Test a subset due to firmware limitations (only first few params returned)
    expected_params = [
        "leg0_coxa_offset", "leg0_coxa_invert", "leg0_coxa_min",
        "leg0_coxa_max", "leg0_coxa_pwm_min", "leg0_coxa_pwm_max",
        "leg0_coxa_neutral", "leg0_femur_offset"
    ]
    for param in expected_params:
        assert param in response, (
            f"Expected joint_cal parameter '{param}' in list output, got: {response!r}"
        )

@pytest.mark.parametrize("leg,joint,param_type", [
    (leg, joint, ptype) for leg in range(6) for joint in JOINT_NAMES for ptype in PARAM_TYPES_FLOAT
])
def test_joint_cal_get_returns_float(send_rpc, leg, joint, param_type):
    param_name = f"leg{leg}_{joint}_{param_type}"
    command = f"get joint_cal {param_name}"
    response = send_rpc(command)
    _assert_rpc_success(response, command)
    value = _parse_kv(response, param_name)
    # Accept floats like 0.068, -0.1, 1.0, etc.
    assert re.match(r"^-?\d+\.\d+$", value), (
        f"Expected {param_name} to match float pattern, got value {value!r} from {response!r}"
    )

@pytest.mark.parametrize("leg,joint,param_type", [
    (leg, joint, ptype) for leg in range(6) for joint in JOINT_NAMES for ptype in PARAM_TYPES_INT
])
def test_joint_cal_get_returns_integer(send_rpc, leg, joint, param_type):
    param_name = f"leg{leg}_{joint}_{param_type}"
    command = f"get joint_cal {param_name}"
    response = send_rpc(command)
    _assert_rpc_success(response, command)
    value = _parse_kv(response, param_name)
    # Accept integers like -1, 0, 1, 1500, etc.
    assert re.match(r"^-?\d+$", value), (
        f"Expected {param_name} to match integer pattern, got value {value!r} from {response!r}"
    )

# Test set/get for a float parameter (e.g., leg0_coxa_offset)
def test_joint_cal_set_mem_updates_value_and_restores_float(send_rpc):
    param_name = "leg0_coxa_offset"
    get_cmd = f"get joint_cal {param_name}"
    original_response = send_rpc(get_cmd)
    _assert_rpc_success(original_response, get_cmd)
    original_value = _parse_kv(original_response, param_name)
    try:
        new_value = "0.1" if original_value != "0.1" else "0.2"
        set_cmd = f"set joint_cal {param_name} {new_value}"
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
        restore_cmd = f"set joint_cal {param_name} {original_value}"
        restore_response = send_rpc(restore_cmd)
        _assert_rpc_success(restore_response, restore_cmd)

# Test set/get for an int parameter (e.g., leg0_coxa_pwm_min)
def test_joint_cal_set_mem_updates_value_and_restores_int(send_rpc):
    param_name = "leg0_coxa_pwm_min"
    get_cmd = f"get joint_cal {param_name}"
    original_response = send_rpc(get_cmd)
    _assert_rpc_success(original_response, get_cmd)
    original_value = _parse_kv(original_response, param_name)
    try:
        new_value = "1000" if original_value != "1000" else "1100"
        set_cmd = f"set joint_cal {param_name} {new_value}"
        set_response = send_rpc(set_cmd)
        _assert_rpc_success(set_response, set_cmd)
        assert f"{param_name}={new_value}" in set_response, (
            f"Expected set echo in response, got: {set_response!r}"
        )
        assert "(mem)" in set_response, f"Expected mem set marker, got: {set_response!r}"
        verify_response = send_rpc(get_cmd)
        _assert_rpc_success(verify_response, get_cmd)
        assert _parse_kv(verify_response, param_name) == new_value, (
            f"Expected {param_name}={new_value}, got: {verify_response!r}"
        )
    finally:
        restore_cmd = f"set joint_cal {param_name} {original_value}"
        restore_response = send_rpc(restore_cmd)
        _assert_rpc_success(restore_response, restore_cmd)

@pytest.mark.parametrize(
    "param_name,invalid_value",
    [
        ("leg0_coxa_offset", "-100.0"),
        ("leg0_coxa_offset", "100.0"),
        ("leg0_coxa_min", "-100.0"),
        ("leg0_coxa_max", "100.0"),
        ("leg0_coxa_pwm_min", "-10000"),
        ("leg0_coxa_pwm_max", "10000"),
    ],
)
def test_joint_cal_set_rejects_out_of_range_values(send_rpc, param_name, invalid_value):
    command = f"set joint_cal {param_name} {invalid_value}"
    response = send_rpc(command)
    assert response.strip(), f"Expected non-empty response for command: {command}"
    assert "ERROR set" in response, (
        f"Expected out-of-range rejection for {param_name}={invalid_value}, got: {response!r}"
    )

def test_joint_cal_setpersist_persists_and_can_be_restored(send_rpc):
    param_name = "leg0_coxa_min"
    get_cmd = f"get joint_cal {param_name}"
    original_response = send_rpc(get_cmd)
    _assert_rpc_success(original_response, get_cmd)
    original_value = float(_parse_kv(original_response, param_name))
    candidate_values = [-0.5, -0.4, -0.3]
    new_value = next((v for v in candidate_values if abs(v - original_value) > 1e-6), None)
    assert new_value is not None, "Failed to find alternate test value"
    setpersist_cmd = f"setpersist joint_cal {param_name} {new_value}"
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
        restore_cmd = f"setpersist joint_cal {param_name} {original_value}"
        restore_response = send_rpc(restore_cmd)
        _assert_rpc_success(restore_response, restore_cmd)