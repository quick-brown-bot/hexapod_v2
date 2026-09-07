#!/usr/bin/env python3
"""Enumerate which LegBoards are answering on the RS485 bus and print each
one's actually-reported joint angles, by asking the mainboard's RPC `pos
<leg>` command for legs 1-6 in turn.

`pos` makes the mainboard request fresh joint-position telemetry from that
leg over RS485 and report it back, so this is ground truth for "is the leg
wired up and talking", not just "did the mainboard try to poll it". Useful
during bring-up (often only one leg is connected, and not necessarily leg
1) and as a quick bus health check.

Usage:
    python3 tools/leg_scan.py
    python3 tools/leg_scan.py --host 192.168.4.1 --port 5555
    python3 tools/leg_scan.py --watch          # re-scan continuously

Connects over WiFi TCP (connect to the robot AP first). The mainboard
accepts one RPC client at a time, so close other RPC tools while this runs.
"""
import argparse
import socket
import time


def rpc(sock, cmd, timeout=3.0):
    sock.sendall((cmd + "\n").encode())
    sock.settimeout(timeout)
    chunks = []
    try:
        # `pos` answers in a single line; one recv is enough, but keep
        # reading briefly in case the response is split across packets.
        while True:
            data = sock.recv(4096)
            if not data:
                break
            chunks.append(data)
            if b"\n" in data:
                break
    except socket.timeout:
        pass
    return b"".join(chunks).decode(errors="replace").strip()


def parse_pos(line):
    """Returns (state, detail) where state is one of 'ok', 'absent', 'stale'.

    Expected `pos <leg>:` replies (see hex_rpc_core/rpc_commands.c cmd_pos):
      - "pos N: leg has never responded"
      - "pos N: stale=.. status=.. (no position data after retrying -- try again)"
      - "pos N: stale=0 coxa=.. femur=.. tibia=.."
    """
    body = line.split(":", 1)[1].strip() if ":" in line else line
    if "has never responded" in body:
        return "absent", body
    fields = {}
    for tok in body.split():
        if "=" in tok:
            k, v = tok.split("=", 1)
            fields[k] = v
    if "coxa" in fields:
        stale = fields.get("stale", "?")
        detail = f"coxa={fields['coxa']:>7}  femur={fields['femur']:>7}  tibia={fields['tibia']:>7}"
        if stale not in ("0", "?"):
            detail += f"  (stale={stale})"
        return "ok", detail
    return "stale", body


def scan(sock):
    rows = []
    for leg in range(1, 7):
        line = rpc(sock, f"pos {leg}")
        if not line:
            rows.append((leg, "timeout", "no reply from mainboard RPC"))
            continue
        state, detail = parse_pos(line)
        rows.append((leg, state, detail))
    return rows


def print_scan(rows):
    mark = {"ok": "  UP  ", "absent": " ---- ", "stale": " STALE", "timeout": " ???? "}
    up = 0
    for leg, state, detail in rows:
        if state == "ok":
            up += 1
        print(f"  leg {leg}  [{mark.get(state, state)}]  {detail}")
    print(f"  {up}/6 legs responding")


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default="192.168.4.1")
    ap.add_argument("--port", type=int, default=5555)
    ap.add_argument("--watch", action="store_true",
                    help="re-scan continuously until interrupted")
    ap.add_argument("--interval", type=float, default=1.0,
                    help="seconds between scans with --watch (default: 1.0)")
    args = ap.parse_args()

    sock = socket.create_connection((args.host, args.port), timeout=5)
    try:
        print(rpc(sock, "version"))
        while True:
            print(f"--- {time.strftime('%H:%M:%S')} ---")
            print_scan(scan(sock))
            if not args.watch:
                break
            time.sleep(args.interval)
    except KeyboardInterrupt:
        pass
    finally:
        sock.close()


if __name__ == "__main__":
    main()
