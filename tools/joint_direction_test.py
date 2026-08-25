#!/usr/bin/env python3
"""Interactive RPC joint-direction test for one LegBoard.

Drives the mainboard's WiFi TCP RPC transport (default 192.168.4.1:5555,
the AP the mainboard itself brings up) -- so this exercises the full
mainboard -> RS485 -> LegBoard path end to end, not a direct LegBoard USB
connection. Uses the `joint <leg> <coxa_deg> <femur_deg> <tibia_deg>` RPC
command, which overrides the gait/IK output for that leg's three joints
until released (`joint <leg> release`), so a single joint can be exercised
directly without the 100 Hz gait loop fighting it.

For each joint in turn: commands a positive angle, asks you to describe
which way it moved, returns to neutral, then the same for a negative angle.
Prints a summary of your answers at the end -- this writes the sign
convention down instead of it being re-derived from memory each time.

Leg-local IK frame for reference (see hex_kinematics/leg.c): X out from the
body, Y forward, Z up. Coxa yaw is straightforward (positive = which way?
that's what this script asks). Femur/tibia sign vs. "up"/"down" is NOT
obvious from the IK code alone (interior-angle geometry) -- that's exactly
why this script asks rather than assumes.

IK-driven leg-tip testing (Cartesian x/y/z targets, once leg_geom is
recalibrated -- see docs/development/LEG_CALIBRATION.md) is a planned
follow-on, not implemented here yet. See test_ik() below.

Usage:
    python3 tools/joint_direction_test.py
    python3 tools/joint_direction_test.py --leg 1 --angle 25
    python3 tools/joint_direction_test.py --host 192.168.4.1 --port 5555
"""
import argparse
import socket
import time

JOINTS = ["coxa", "femur", "tibia"]


def rpc_send(sock, cmd):
    sock.sendall((cmd + "\n").encode())
    time.sleep(0.15)
    sock.settimeout(1.0)
    buf = b""
    try:
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            buf += chunk
    except socket.timeout:
        pass
    return buf.decode(errors="replace").strip()


def detect_first_available_leg(sock):
    """Picks the first leg (1-6) that has ever responded on the RS485 bus,
    via `pos`, rather than assuming leg 1 is the one wired up -- during
    bring-up/calibration it's common for only one leg to be connected, and
    not necessarily leg 1."""
    for leg in range(1, 7):
        if "has never responded" not in rpc_send(sock, f"pos {leg}"):
            return leg
    raise SystemExit("no leg responded on the RS485 bus (checked legs 1-6)")


def set_joint(sock, leg, coxa, femur, tibia):
    return rpc_send(sock, f"joint {leg} {coxa:.1f} {femur:.1f} {tibia:.1f}")


def release(sock, leg):
    return rpc_send(sock, f"joint {leg} release")


def test_joint(sock, leg, joint_index, angle_deg):
    name = JOINTS[joint_index]
    results = {}
    for sign, label in [(1, "positive"), (-1, "negative")]:
        angles = [0.0, 0.0, 0.0]
        angles[joint_index] = sign * angle_deg
        set_joint(sock, leg, *angles)
        print(f"\n{name}: commanded {label} ({sign * angle_deg:+.0f} deg)")
        direction = input("  which way did it move? ").strip()
        results[label] = direction or "(no answer)"
        set_joint(sock, leg, 0.0, 0.0, 0.0)
        time.sleep(0.3)
    return results


def test_ik(sock, leg):
    # Planned follow-on: drive the leg tip via Cartesian (x, y, z) targets
    # through IK instead of raw joint angles, once leg_geom is recalibrated
    # for the real frame (it's currently at generic template defaults --
    # see docs/development/LEG_CALIBRATION.md). Needs a mainboard RPC command
    # that isn't implemented yet (only per-joint override exists today).
    raise NotImplementedError("IK leg-tip test: not implemented yet")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default="192.168.4.1", help="mainboard RPC host (its own WiFi AP IP by default)")
    ap.add_argument("--port", type=int, default=5555)
    ap.add_argument("--leg", type=int, default=None,
                     help="leg number, 1-6 (default: first leg that responds on the bus)")
    ap.add_argument("--angle", type=float, default=20.0, help="test angle in degrees, one joint at a time")
    args = ap.parse_args()

    print(f"Connecting to mainboard RPC at {args.host}:{args.port} ...")
    sock = socket.create_connection((args.host, args.port), timeout=5)
    print(rpc_send(sock, "version"))

    if args.leg is None:
        args.leg = detect_first_available_leg(sock)
        print(f"--leg not given; auto-detected leg {args.leg} as first available")

    summary = {}
    try:
        for i, name in enumerate(JOINTS):
            input(f"\n=== {name} === press Enter to start (leg {args.leg} will move) ")
            summary[name] = test_joint(sock, args.leg, i, args.angle)
    finally:
        release(sock, args.leg)
        sock.close()

    print("\n=== Summary ===")
    for name, res in summary.items():
        print(f"  {name}: +{args.angle:.0f} deg -> {res['positive']}   "
              f"-{args.angle:.0f} deg -> {res['negative']}")


if __name__ == "__main__":
    main()
