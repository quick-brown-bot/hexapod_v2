#!/usr/bin/env python3
"""Set one leg's foot to an absolute Cartesian target via the mainboard's
RPC `ik <leg> <x> <y> <z>` command (leg-local frame: X out, Y forward, Z up),
then read back the LegBoard's actually-reported joint angles (`pos`) for
comparison against what was commanded.

Usage:
    python3 tools/ik.py <x> <y> <z> [--leg N] [--release]
    python3 tools/ik.py 0.148 0 -0.127     # this leg's neutral pose
    python3 tools/ik.py 0.02 0 0           # a specific test point

The leg holds the commanded position after this exits (RPC `joint`/`ik`
override behavior) unless --release is passed.
"""
import argparse
import socket
import time


def rpc(host, port, cmd, timeout=3.0):
    s = socket.create_connection((host, port), timeout=5)
    s.sendall((cmd + "\n").encode())
    s.settimeout(timeout)
    try:
        data = s.recv(4096)
    except socket.timeout:
        data = b""
    s.close()
    return data.decode(errors="replace").strip()


def detect_first_available_leg(host, port):
    """Picks the first leg (1-6) that has ever responded on the RS485 bus,
    via `pos`, rather than assuming leg 1 is the one wired up -- during
    bring-up/calibration it's common for only one leg to be connected, and
    not necessarily leg 1."""
    for leg in range(1, 7):
        if "has never responded" not in rpc(host, port, f"pos {leg}"):
            return leg
    raise SystemExit("no leg responded on the RS485 bus (checked legs 1-6)")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("x", type=float)
    ap.add_argument("y", type=float)
    ap.add_argument("z", type=float)
    ap.add_argument("--leg", type=int, default=None,
                     help="leg number, 1-6 (default: first leg that responds on the bus)")
    ap.add_argument("--host", default="192.168.4.1")
    ap.add_argument("--port", type=int, default=5555)
    ap.add_argument("--release", action="store_true",
                     help="release the override right after (default: hold the position)")
    args = ap.parse_args()

    if args.leg is None:
        args.leg = detect_first_available_leg(args.host, args.port)
        print(f"--leg not given; auto-detected leg {args.leg} as first available")

    print(rpc(args.host, args.port, f"ik {args.leg} {args.x} {args.y} {args.z}"))
    time.sleep(0.3)  # let the servo settle before reading position back
    print(rpc(args.host, args.port, f"pos {args.leg}", timeout=3.0))
    if args.release:
        print(rpc(args.host, args.port, f"joint {args.leg} release"))


if __name__ == "__main__":
    main()
