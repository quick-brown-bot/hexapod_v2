#!/usr/bin/env python3
"""Send RPC command(s) to the mainboard over WiFi TCP and print the reply.

The mainboard exposes its text RPC only over WiFi TCP (and Bluetooth SPP) --
not the USB serial console, which is log output only. This is the generic
way to poke it: `get`/`set`/`save`/`list`/`joint`/`ik`/`pos`/... See
docs/interfaces/RPC_USER_GUIDE.md for the command set.

Usage:
    python3 tools/rpc.py "get controller flysky_rx_gpio"
    python3 tools/rpc.py "set controller flysky_rx_gpio 34" "save controller"
    python3 tools/rpc.py --host 192.168.4.1 --port 5555 "list namespaces"
    python3 tools/rpc.py -i          # interactive line-by-line prompt
"""
import argparse
import socket
import sys
import time


def rpc(sock, cmd, settle=0.4):
    sock.sendall((cmd + "\n").encode())
    time.sleep(settle)
    sock.settimeout(3.0)
    out = b""
    try:
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            out += chunk
            if len(chunk) < 4096:
                break
    except socket.timeout:
        pass
    return out.decode(errors="replace").strip()


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("commands", nargs="*", help="one or more RPC command strings")
    ap.add_argument("--host", default="192.168.4.1")
    ap.add_argument("--port", type=int, default=5555)
    ap.add_argument("-i", "--interactive", action="store_true",
                    help="prompt for commands line by line until EOF/blank")
    args = ap.parse_args()

    sock = socket.create_connection((args.host, args.port), timeout=5)
    try:
        if args.interactive:
            while True:
                try:
                    line = input("rpc> ").strip()
                except EOFError:
                    break
                if not line:
                    break
                print(rpc(sock, line))
        else:
            if not args.commands:
                print("no command given", file=sys.stderr)
                sys.exit(2)
            for cmd in args.commands:
                reply = rpc(sock, cmd)
                print(f"$ {cmd}\n{reply}" if reply else f"$ {cmd}\n(no reply)")
    finally:
        sock.close()


if __name__ == "__main__":
    main()
