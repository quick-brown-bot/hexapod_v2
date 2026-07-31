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

NUM_LEGS = 6
PARAM_SUFFIXES = [
    "len_coxa", "len_femur", "len_tibia",
    "mount_x", "mount_y", "mount_z", "mount_yaw",
    "stance_out", "stance_fwd"
]


def test_leg_geom_param_in_list_limited(send_rpc):
    response = send_rpc("list leg_geom")
    _assert_rpc_success(response, "list leg_geom")
    # Only check for all leg0 params and leg1_len_coxa, matching firmware output
    for suffix in PARAM_SUFFIXES:
        param_name = f"leg0_{suffix}"
        assert param_name in response, f"Expected {param_name} in list output, got: {response!r}"
    assert "leg1_len_coxa" in response, f"Expected leg1_len_coxa in list output, got: {response!r}"

@pytest.mark.parametrize("leg,param_suffix", [
    (leg, suffix) for leg in range(NUM_LEGS) for suffix in PARAM_SUFFIXES
])
def test_leg_geom_get_returns_float(send_rpc, leg, param_suffix):
    param_name = f"leg{leg}_{param_suffix}"
    command = f"get leg_geom {param_name}"
    response = send_rpc(command)
    _assert_rpc_success(response, command)
    value = _parse_kv(response, param_name)
    # Accept floats like 0.068, -0.1, 1.0, etc.
    assert re.match(r"^-?\d+\.\d+$", value), (
        f"Expected {param_name} to match float pattern, got value {value!r} from {response!r}"
    )

def test_leg_geom_namespace_appears_in_namespace_listing(send_rpc):
    response = send_rpc("list namespaces")
    _assert_rpc_success(response, "list namespaces")
    assert "leg_geom" in response, f"Expected 'leg_geom' namespace in response, got: {response!r}"

# Test set/get for a parameter (e.g., leg0_len_coxa)
def test_leg_geom_set_mem_updates_value_and_restores(send_rpc):
    param_name = "leg0_len_coxa"
    get_cmd = f"get leg_geom {param_name}"
    original_response = send_rpc(get_cmd)
    _assert_rpc_success(original_response, get_cmd)
    original_value = _parse_kv(original_response, param_name)
    try:
        new_value = "0.07" if original_value != "0.07" else "0.08"
        set_cmd = f"set leg_geom {param_name} {new_value}"
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
        restore_cmd = f"set leg_geom {param_name} {original_value}"
        restore_response = send_rpc(restore_cmd)
        _assert_rpc_success(restore_response, restore_cmd)

@pytest.mark.parametrize("param_name,invalid_value", [
    ("leg0_len_coxa", "0.001"),
    ("leg0_len_femur", "2.0"),
    ("leg0_mount_x", "-2.0"),
    ("leg0_mount_yaw", "7.0"),
])
def test_leg_geom_set_rejects_out_of_range_values(send_rpc, param_name, invalid_value):
    command = f"set leg_geom {param_name} {invalid_value}"
    response = send_rpc(command)
    assert response.strip(), f"Expected non-empty response for command: {command}"
    assert "ERROR set" in response, (
        f"Expected out-of-range rejection for {param_name}={invalid_value}, got: {response!r}"
    )

def test_leg_geom_setpersist_persists_and_can_be_restored(send_rpc):
    param_name = "leg0_len_femur"
    get_cmd = f"get leg_geom {param_name}"
    original_response = send_rpc(get_cmd)
    _assert_rpc_success(original_response, get_cmd)
    original_value = float(_parse_kv(original_response, param_name))
    candidate_values = [0.09, 0.10, 0.11, 0.12]
    new_value = next((v for v in candidate_values if abs(v - original_value) > 1e-6), None)
    assert new_value is not None, "Failed to find alternate test value"
    setpersist_cmd = f"setpersist leg_geom {param_name} {new_value}"
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
        restore_cmd = f"setpersist leg_geom {param_name} {original_value}"
        restore_response = send_rpc(restore_cmd)
        _assert_rpc_success(restore_response, restore_cmd)
