#!/usr/bin/env python3
"""Line-buffered serial console: local echo, sends only on Enter.

`pio device monitor` runs in raw mode -- it doesn't locally echo what you
type and sends each keystroke immediately instead of a full line. That's
awkward for the interactive USB-CDC console in this repo (firmware/leg's
calib.cpp), which reads one '\n'-terminated line at a time. This script uses
input(), so the terminal itself echoes what you type, and only sends when
you press Enter. Everything the board sends is printed as it arrives, from a
background reader thread.

Usage:
    python tools/serial_console.py [--port COM10] [--baud 115200]

If --port is omitted and exactly one serial port is found, it's used
automatically; otherwise you'll be prompted to pick one.

Requires pyserial. If you don't have it in your default Python, PlatformIO
ships its own copy -- run this with its interpreter instead, no extra
install needed:
    ~/.platformio/penv/Scripts/python.exe tools/serial_console.py
"""
import argparse
import sys
import threading

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print(
        "pyserial not found. Either `pip install pyserial`, or run this "
        "script with PlatformIO's bundled interpreter, e.g.:\n"
        "  ~/.platformio/penv/Scripts/python.exe tools/serial_console.py",
        file=sys.stderr,
    )
    sys.exit(1)


# USB VID of the RP2040's built-in CDC (Raspberry Pi Foundation) -- what a
# XIAO RP2040 (LegBoard) enumerates as. Preferred when picking automatically
# so unrelated ports (e.g. Windows' Bluetooth virtual COM ports) don't count
# against the "exactly one port" auto-pick.
RP2040_USB_VID = 0x2E8A


def pick_port() -> str:
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("No serial ports found.", file=sys.stderr)
        sys.exit(1)

    rp2040_ports = [p for p in ports if p.vid == RP2040_USB_VID]
    candidates = rp2040_ports or ports

    if len(candidates) == 1:
        p = candidates[0]
        print(f"Auto-detected port: {p.device} ({p.description})")
        return p.device

    print("Available ports:")
    for i, p in enumerate(candidates):
        print(f"  [{i}] {p.device}  {p.description}")
    idx = input("Select port: ").strip()
    return candidates[int(idx)].device


def reader_thread(ser: "serial.Serial") -> None:
    while True:
        try:
            data = ser.read(ser.in_waiting or 1)
        except serial.SerialException:
            break
        if data:
            sys.stdout.write(data.decode(errors="replace"))
            sys.stdout.flush()


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", help="serial port, e.g. COM10 (prompts if omitted and ambiguous)")
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    port = args.port or pick_port()
    ser = serial.Serial(port, args.baud, timeout=0.1)
    print(f"Connected to {port} @ {args.baud}. Type a line and press Enter. Ctrl+C to quit.\n")

    t = threading.Thread(target=reader_thread, args=(ser,), daemon=True)
    t.start()

    try:
        while True:
            line = input()
            ser.write((line + "\n").encode())
    except (KeyboardInterrupt, EOFError):
        pass
    finally:
        ser.close()
        print("\nDisconnected.")


if __name__ == "__main__":
    main()
