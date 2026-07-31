import os
import re
import socket
import time
from typing import Optional

import pytest

from winwifi import WinWiFi


HEXAPOD_SSID_PATTERN = re.compile(r"^HEXAPOD_.+")


def _get_current_ssid() -> Optional[str]:
    try:
        connected_interfaces = WinWiFi.get_connected_interfaces()
    except Exception:
        return None

    for interface in connected_interfaces:
        ssid = getattr(interface, "ssid", None)
        if ssid:
            return ssid
    return None


def _find_first_hexapod_ssid() -> Optional[str]:
    available_aps = WinWiFi.scan()

    for ap in available_aps:
        ssid = getattr(ap, "ssid", None)
        if ssid and HEXAPOD_SSID_PATTERN.match(ssid):
            return ssid
    return None


def _ensure_hexapod_wifi() -> str:
    current_ssid = _get_current_ssid()
    if current_ssid and HEXAPOD_SSID_PATTERN.match(current_ssid):
        return current_ssid

    discovered_ssid = _find_first_hexapod_ssid()
    if not discovered_ssid:
        pytest.skip("No HEXAPOD_xxx Wi-Fi network found during scan")

    if WinWiFi is None:
        pytest.skip("winwifi is unavailable, cannot connect to HEXAPOD_xxx")
    
    try:
        WinWiFi.connect(discovered_ssid, passwd="", remember=True)
    except Exception as exc:
        pytest.skip(f"Failed to connect to {discovered_ssid}: {exc}")

    time.sleep(2)
    return discovered_ssid


@pytest.fixture(scope="session")
def robot_network() -> dict:
    if os.name != "nt":
        pytest.skip("These integration tests require Windows Wi-Fi control")

    connected_ssid = _ensure_hexapod_wifi()
    return {
        "ssid": connected_ssid,
        "ip": os.getenv("HEXAPOD_IP", "192.168.4.1"),
        "port": int(os.getenv("HEXAPOD_PORT", "5555")),
    }


@pytest.fixture
def rpc_socket(robot_network: dict):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5)
    sock.connect((robot_network["ip"], robot_network["port"]))
    try:
        yield sock
    finally:
        sock.close()


@pytest.fixture
def send_rpc(rpc_socket):
    def _send(command: str, recv_size: int = 4096) -> str:
        if not command.endswith("\n"):
            command_to_send = f"{command}\n"
        else:
            command_to_send = command

        rpc_socket.send(command_to_send.encode("utf-8"))
        try:
            response = rpc_socket.recv(recv_size).decode(errors="ignore")
        except socket.timeout:
            pytest.fail(f"RPC timeout for command: {command.strip()}")
        return response

    return _send