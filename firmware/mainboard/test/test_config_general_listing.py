import re


def test_list_namespaces_returns_non_empty_response(send_rpc):
    response = send_rpc("list namespaces")
    assert response.strip(), "Expected non-empty response for 'list namespaces'"


def test_list_namespaces_contains_at_least_one_known_namespace(send_rpc):
    response = send_rpc("list namespaces")
    assert any(
        ns in response for ns in ["system", "joint_cal", "leg_geom", "motion_lim", "servo_map"]
    ), f"Expected at least one known namespace in response, got: {response!r}"


def test_list_command_accepts_generic_listing_form(send_rpc):
    response = send_rpc("list")
    assert response.strip(), "Expected non-empty response for generic 'list' command"
    assert re.search(r"list|namespace|param|usage", response, flags=re.IGNORECASE), (
        f"Expected listing/help-oriented output, got: {response!r}"
    )