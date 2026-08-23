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


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("x", type=float)
    ap.add_argument("y", type=float)
    ap.add_argument("z", type=float)
    ap.add_argument("--leg", type=int, default=1, help="leg number, 1-6 (default 1)")
    ap.add_argument("--host", default="192.168.4.1")
    ap.add_argument("--port", type=int, default=5555)
    ap.add_argument("--release", action="store_true",
                     help="release the override right after (default: hold the position)")
    args = ap.parse_args()

    print(rpc(args.host, args.port, f"ik {args.leg} {args.x} {args.y} {args.z}"))
    time.sleep(0.3)  # let the servo settle before reading position back
    print(rpc(args.host, args.port, f"pos {args.leg}", timeout=3.0))
    if args.release:
        print(rpc(args.host, args.port, f"joint {args.leg} release"))


if __name__ == "__main__":
    main()
