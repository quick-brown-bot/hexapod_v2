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
PARAM_TYPES = ["gpio", "driver"]

def test_servo_map_namespace_appears_in_namespace_listing(send_rpc):
    response = send_rpc("list namespaces")
    _assert_rpc_success(response, "list namespaces")
    assert "servo_map" in response, f"Expected 'servo_map' namespace in response, got: {response!r}"

def test_servo_map_list_includes_core_parameters(send_rpc):
    response = send_rpc("list servo_map")
    _assert_rpc_success(response, "list servo_map")
    assert "list servo_map:" in response, f"Expected servo_map listing prefix, got: {response!r}"
    # Test a subset due to firmware limitations (only first few params returned)
    expected_params = [
        "leg0_group", "leg0_coxa_gpio", "leg0_coxa_driver",
        "leg0_femur_gpio", "leg0_femur_driver", "leg0_tibia_gpio"
    ]
    for param in expected_params:
        assert param in response, (
            f"Expected servo_map parameter '{param}' in list output, got: {response!r}"
        )

@pytest.mark.parametrize("leg,joint,param_type", [
    (leg, joint, ptype) for leg in range(6) for joint in JOINT_NAMES for ptype in PARAM_TYPES
] + [(leg, None, "group") for leg in range(6)])
def test_servo_map_get_returns_integer(send_rpc, leg, joint, param_type):
    if param_type == "group":
        param_name = f"leg{leg}_group"
    else:
        param_name = f"leg{leg}_{joint}_{param_type}"
    command = f"get servo_map {param_name}"
    response = send_rpc(command)
    _assert_rpc_success(response, command)
    value = _parse_kv(response, param_name)
    # Accept integers like -1, 0, 1, 27, etc.
    assert re.match(r"^-?\d+$", value), (
        f"Expected {param_name} to match integer pattern, got value {value!r} from {response!r}"
    )

# Test set/get for a parameter (e.g., leg0_coxa_gpio)
def test_servo_map_set_mem_updates_value_and_restores(send_rpc):
    param_name = "leg0_coxa_gpio"
    get_cmd = f"get servo_map {param_name}"
    original_response = send_rpc(get_cmd)
    _assert_rpc_success(original_response, get_cmd)
    original_value = _parse_kv(original_response, param_name)
    try:
        new_value = "28" if original_value != "28" else "29"
        set_cmd = f"set servo_map {param_name} {new_value}"
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
        restore_cmd = f"set servo_map {param_name} {original_value}"
        restore_response = send_rpc(restore_cmd)
        _assert_rpc_success(restore_response, restore_cmd)

@pytest.mark.parametrize(
    "param_name,invalid_value",
    [
        ("leg0_group", "-1"),
        ("leg0_group", "2"),
        ("leg0_coxa_gpio", "-2"),
        ("leg0_coxa_driver", "-1"),
        ("leg0_coxa_driver", "2"),
    ],
)
def test_servo_map_set_rejects_out_of_range_values(send_rpc, param_name, invalid_value):
    command = f"set servo_map {param_name} {invalid_value}"
    response = send_rpc(command)
    assert response.strip(), f"Expected non-empty response for command: {command}"
    assert "ERROR set" in response, (
        f"Expected out-of-range rejection for {param_name}={invalid_value}, got: {response!r}"
    )

def test_servo_map_setpersist_persists_and_can_be_restored(send_rpc):
    param_name = "leg0_group"
    get_cmd = f"get servo_map {param_name}"
    original_response = send_rpc(get_cmd)
    _assert_rpc_success(original_response, get_cmd)
    original_value = int(_parse_kv(original_response, param_name))
    candidate_values = [0, 1]
    new_value = next((v for v in candidate_values if v != original_value), None)
    assert new_value is not None, "Failed to find alternate test value"
    setpersist_cmd = f"setpersist servo_map {param_name} {new_value}"
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
        restore_cmd = f"setpersist servo_map {param_name} {original_value}"
        restore_response = send_rpc(restore_cmd)
        _assert_rpc_success(restore_response, restore_cmd)