#!/usr/bin/env python3
"""Live current-sense voltage monitor for a LegBoard, over its own USB
serial console (firmware/leg/src/calib.cpp's CURRAW?/CURCAL?/CURFILT?).
Polls all 4 channels and redraws one status line in place (via '\\r') -- no
scrolling, just a live readout, handy for manually checking wiring/behavior
(this is how the coxa/femur/tibia ADC pin-mapping mismatch got caught -- see
docs/architecture/HARDWARE_AND_MECHANICS.md).

The active current-smoothing strategy (CURFILT, EMA or BOXCAR N) is queried
and printed once -- it doesn't need to live-update on the same line, it
rarely changes mid-session. On Windows, press 'f' at any point during the
live readout to change it interactively (pauses the line, prompts, then
resumes); use --filter/--n to set it once at startup instead (works
everywhere, no keypress polling needed).

Usage:
    python tools/legboard_current_monitor.py                # auto-detects the port
    python tools/legboard_current_monitor.py --port COM10
    python tools/legboard_current_monitor.py --interval 0.1  # faster refresh
    python tools/legboard_current_monitor.py --filter boxcar --n 10
    python tools/legboard_current_monitor.py --filter ema

Requires pyserial -- if you don't have it in your default Python, run this
with PlatformIO's bundled interpreter instead:
    ~/.platformio/penv/Scripts/python.exe tools/legboard_current_monitor.py

Ctrl+C to quit.
"""
import argparse
import sys
import time

if sys.platform == "win32":
    import msvcrt
else:
    msvcrt = None

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print(
        "pyserial not found. Either `pip install pyserial`, or run this "
        "script with PlatformIO's bundled interpreter, e.g.:\n"
        "  ~/.platformio/penv/Scripts/python.exe tools/legboard_current_monitor.py",
        file=sys.stderr,
    )
    sys.exit(1)

# USB VID of the RP2040's built-in CDC (Raspberry Pi Foundation) -- what a
# XIAO RP2040 (LegBoard) enumerates as.
RP2040_USB_VID = 0x2E8A

# CH0..CH3, matching calib.cpp's s_channel_name[] / CURRAW?/CURCAL? order.
CHANNEL_NAMES = ["total", "coxa", "femur", "tibia"]


def autodetect_port() -> str:
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("No serial ports found. Is the LegBoard plugged in?", file=sys.stderr)
        sys.exit(1)

    rp2040_ports = [p for p in ports if p.vid == RP2040_USB_VID]
    candidates = rp2040_ports or ports

    if len(candidates) == 1:
        p = candidates[0]
        print(f"Auto-detected port: {p.device} ({p.description})")
        return p.device

    print("Multiple candidate ports found:")
    for i, p in enumerate(candidates):
        print(f"  [{i}] {p.device}  {p.description}")
    idx = input("Select port: ").strip()
    return candidates[int(idx)].device


class LegLink:
    """Thin synchronous request/response wrapper over calib.cpp's line console."""

    def __init__(self, port: str, baud: int = 115200, timeout: float = 2.0):
        self.ser = serial.Serial(port, baud, timeout=timeout)
        time.sleep(0.3)
        self.ser.reset_input_buffer()

    def command(self, cmd: str, expect_lines: int = 1, timeout: float = 2.0):
        self.ser.reset_input_buffer()
        self.ser.write((cmd + "\n").encode())
        lines = []
        deadline = time.time() + timeout
        while len(lines) < expect_lines and time.time() < deadline:
            raw = self.ser.readline()
            if not raw:
                continue
            line = raw.decode(errors="replace").strip()
            if line:
                lines.append(line)
        return lines


def parse_curraw(lines) -> dict:
    """{channel_index: (raw_counts, raw_mv)} from CURRAW? output."""
    out = {}
    for line in lines:
        if not line.startswith("CH"):
            continue
        toks = line.split()
        ch = int(toks[0][2:])
        counts = mv = None
        for t in toks:
            if t.startswith("counts="):
                counts = int(t[len("counts="):])
            elif t.startswith("mv="):
                mv = float(t[len("mv="):])
        if counts is not None and mv is not None:
            out[ch] = (counts, mv)
    return out


def parse_curcal(lines) -> dict:
    """{channel_index: (scale_ma_per_mv, offset_ma)} from CURCAL? output."""
    out = {}
    for line in lines:
        if not line.startswith("CH"):
            continue
        toks = line.split()
        ch = int(toks[0][2:])
        scale = offset = None
        for t in toks:
            if t.startswith("scale="):
                scale = float(t[len("scale="):])
            elif t.startswith("offset="):
                offset = float(t[len("offset="):])
        if scale is not None and offset is not None:
            out[ch] = (scale, offset)
    return out


def parse_curfilt(line: str):
    """('EMA', alpha) or ('BOXCAR', n) from one CURFILT? response line."""
    if line.startswith("CURFILT=BOXCAR"):
        n = None
        for tok in line.split():
            if tok.startswith("N="):
                n = int(tok[len("N="):])
        return "BOXCAR", n
    if line.startswith("CURFILT=EMA"):
        alpha = None
        for tok in line.split():
            if tok.startswith("ALPHA="):
                alpha = float(tok[len("ALPHA="):])
        return "EMA", alpha
    return None, None


def print_filter_status(link: LegLink) -> None:
    resp = link.command("CURFILT?", expect_lines=1)
    mode, param = parse_curfilt(resp[0]) if resp else (None, None)
    if mode == "BOXCAR":
        print(f"Current smoothing: BOXCAR N={param}")
    elif mode == "EMA":
        print(f"Current smoothing: EMA alpha={param}")
    else:
        print(f"Current smoothing: unknown response {resp}")


def set_filter(link: LegLink, mode: str, param=None) -> None:
    """`param` is the boxcar N (int-ish) or the EMA alpha (float-ish) --
    passed through as text and left for the firmware to validate/clamp."""
    mode = mode.strip().upper()
    if mode == "EMA":
        cmd = "CURFILT EMA" if param in (None, "") else f"CURFILT EMA {param}"
        resp = link.command(cmd, expect_lines=1)
    elif mode == "BOXCAR":
        n = param if param not in (None, "") else 10
        resp = link.command(f"CURFILT BOXCAR {n}", expect_lines=1)
    else:
        print(f"Unknown filter '{mode}', expected 'ema' or 'boxcar'")
        return
    print(f"  {resp[0] if resp else '(no response)'}")


def _f_key_pressed() -> bool:
    """Non-blocking check for the 'f' (change filter) keypress (Windows only)."""
    if msvcrt is None:
        return False
    pressed = False
    while msvcrt.kbhit():
        if msvcrt.getwch().lower() == "f":
            pressed = True
    return pressed


def prompt_change_filter(link: LegLink) -> None:
    """Pause the live line, ask for a new filter, apply it, print the new
    status once, then let the caller resume the live loop."""
    print()  # move off the in-place live-update line first
    s = input("New filter -- 'ema [alpha]' or 'boxcar <n>' (blank to cancel): ").strip()
    if s:
        parts = s.split()
        mode = parts[0]
        param = parts[1] if len(parts) > 1 else None
        set_filter(link, mode, param)
    print_filter_status(link)
    print()


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", help="serial port, e.g. COM10 (auto-detected if omitted)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--interval", type=float, default=0.2, help="poll interval in seconds (default 0.2)")
    ap.add_argument("--filter", choices=["ema", "boxcar"], help="set the smoothing strategy before starting")
    ap.add_argument("--n", type=int, default=10, help="boxcar window size, used only with --filter boxcar (default 10)")
    ap.add_argument("--alpha", type=float, help="EMA alpha (0-1], used only with --filter ema (default: firmware's current value)")
    args = ap.parse_args()

    port = args.port or autodetect_port()
    link = LegLink(port, args.baud)

    ping = link.command("PING", expect_lines=1)
    print(f"Connected: {ping[0] if ping else '(no response)'}")

    calib = parse_curcal(link.command("CURCAL?", expect_lines=4))
    print("Calibration in use (fetched once at startup -- restart this script to refresh):")
    for ch, name in enumerate(CHANNEL_NAMES):
        scale, offset = calib.get(ch, (1.0, 0.0))
        print(f"  CH{ch} {name}: scale={scale:.6f} offset={offset:.3f}")

    if args.filter == "boxcar":
        set_filter(link, "boxcar", args.n)
    elif args.filter == "ema":
        set_filter(link, "ema", args.alpha)
    print_filter_status(link)
    hint = "press 'f' to change it, " if msvcrt is not None else ""
    print(f"({hint}or restart with --filter/--n)")

    print("\nCtrl+C to quit.\n")

    try:
        while True:
            if _f_key_pressed():
                prompt_change_filter(link)

            raw = parse_curraw(link.command("CURRAW?", expect_lines=4))
            parts = []
            for ch, name in enumerate(CHANNEL_NAMES):
                counts, mv = raw.get(ch, (0, 0.0))
                scale, offset = calib.get(ch, (1.0, 0.0))
                ma = mv * scale + offset
                parts.append(f"{name:>5}: {mv:7.2f} mV ({ma:7.1f} mA)")
            sys.stdout.write("\r" + "  |  ".join(parts) + "   ")
            sys.stdout.flush()
            time.sleep(args.interval)
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        link.ser.close()


if __name__ == "__main__":
    main()
