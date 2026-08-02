#!/usr/bin/env python3
"""Interactive LegBoard configurator, driven directly over the board's own
USB serial console (firmware/leg/src/calib.cpp) -- no separate calibration
board or RS485 link needed.

Wizard flow:
  1. Leg address: shows the currently-set address (if any) as the default,
     or prompts for one if the board is uncalibrated (address 0). Confirms
     over `ADDR`.
  2. Prints an ASCII top-view of the board so you know which connector is
     which before touching anything.
  3. Current-sense calibration for each of coxa/femur/tibia in turn:
     a. Zero point: disconnect the servo (open circuit -- no load), read the
        raw ADC millivolt reading (CURRAW?).
     b. Span points: attach each of a few known THT resistor loads in turn,
        enter the measured/computed reference current (mA) for each, read
        the raw ADC reading again.
     c. Fit `current_ma = scale * raw_mv + offset` by ordinary least squares
        over all points (1 zero + N span), then persist it with
        `CURCAL <ch> <scale> <offset>`.

The suggested resistor values stay in the tens-of-mA range on purpose -- the
INA4181 current-shunt amplifier is linear by design, so a handful of
low-power points is enough; there's no need (or safe simple way) to source
amps through a small THT resistor. See docs/development/LEG_CALIBRATION.md.

The `total` channel (0) is intentionally not covered here -- it sees the sum
of all three branches at once, so it can't be isolated with a single test
resistor the way each branch can; see LEG_CALIBRATION.md "Hardware current
range" for its status.

Usage:
    python tools/leg_configurator.py --port COM10
    python tools/leg_configurator.py --port COM10 --channels coxa,femur
    python tools/leg_configurator.py --port COM10 --zero      # build aid: center all 3 servos

Requires pyserial -- if you don't have it in your default Python, run this
with PlatformIO's bundled interpreter instead:
    ~/.platformio/penv/Scripts/python.exe tools/leg_configurator.py --port COM10
"""
import argparse
import sys
import time

try:
    import serial
except ImportError:
    print(
        "pyserial not found. Either `pip install pyserial`, or run this "
        "script with PlatformIO's bundled interpreter, e.g.:\n"
        "  ~/.platformio/penv/Scripts/python.exe tools/leg_configurator.py --port COM10",
        file=sys.stderr,
    )
    sys.exit(1)

# calib.cpp channel indices (ADC_CH_TOTAL/COXA/FEMUR/TIBIA order).
CHANNELS = {"coxa": 1, "femur": 2, "tibia": 3}
# Servo connector per channel, from hardware/legboard/legboard_sch.py
# (J2/COXA_PWM, J3/FEMUR_PWM, J4/TIBIA_PWM) -- see the board diagram below.
CONNECTOR = {"coxa": "J2", "femur": "J3", "tibia": "J4"}
NEUTRAL_PWM_US = 1500  # DEFAULT_PWM_NEUTRAL_US, firmware/leg/src/config.h

# Common E12 THT resistor values, low-to-high current, with the minimum
# power rating each needs at a ~6V servo rail (kept small on purpose).
SPAN_OHMS = [470, 220, 100, 47]
SPAN_POWER = ["1/4W", "1/4W (1/2W recommended)", "1/2W", "1W"]

# Hand-drawn from hardware/legboard/legboard_sch.py placement + the rendered
# board (hardware/legboard/board-front.png) -- component-side top view, not
# to scale. Good enough to tell J2/J3/J4 apart at a glance; not auto-derived
# from the PCB render (see docs/plans/TODO.md for that as a possible follow-up).
BOARD_DIAGRAM = r"""
  Hexapod LegBoard v2.1 -- top view (component side)

  +--------------------------------------------------------------+
  |  J1 UPLINK_RJ11                          J5 SERVO_PWR_IN      |
  |  +--------+                              +---------+          |
  |  | RS485  |            [J2 COXA]         | +6V|GND |          |
  |  |  bus   |             (o)(o)(o)         +---------+         |
  |  +--------+                                                   |
  |                                          [U3 INA4181A3]        |
  |  [U2 SP3485CN]                        (+ shunts R2/R4/R5)      |
  |                                                                |
  |                                                                |
  |  [U1 XIAO RP2040]                                              |
  |  (big 2x14 module,                                             |
  |   bottom-left)                                                 |
  |                                                                |
  |                    [J4 TIBIA]          [J3 FEMUR]              |
  |                     (o)(o)(o)           (o)(o)(o)              |
  +--------------------------------------------------------------+

  J2 = coxa servo/current channel   J3 = femur servo/current channel
  J4 = tibia servo/current channel  J5 = incoming +6V servo power (not per-branch)
"""


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


def read_raw_mv(link: LegLink, ch: int) -> float:
    lines = link.command("CURRAW?", expect_lines=4)
    prefix = f"CH{ch} "
    for line in lines:
        if line.startswith(prefix):
            for tok in line.split():
                if tok.startswith("mv="):
                    return float(tok[len("mv="):])
    raise RuntimeError(f"CURRAW? did not report channel {ch}: {lines}")


def fit_line(raw, ref):
    """Ordinary least squares fit of ref = scale*raw + offset."""
    n = len(raw)
    sx = sum(raw)
    sy = sum(ref)
    sxx = sum(x * x for x in raw)
    sxy = sum(x * y for x, y in zip(raw, ref))
    denom = n * sxx - sx * sx
    if abs(denom) < 1e-9:
        raise RuntimeError("readings are too similar to fit a line")
    scale = (n * sxy - sx * sy) / denom
    offset = (sy - scale * sx) / n
    return scale, offset


def wait_enter(prompt: str) -> None:
    input(prompt + " ")


def read_float(prompt: str) -> float:
    while True:
        s = input(prompt + " ").strip()
        try:
            return float(s)
        except ValueError:
            print("Not a number, try again.")


def parse_addr_response(resp) -> int:
    # "ADDR=<n>"
    if not resp:
        return 0
    try:
        return int(resp[0].split("=", 1)[1])
    except (IndexError, ValueError):
        return 0


def prompt_leg_address(link: LegLink) -> int:
    current = parse_addr_response(link.command("ADDR?", expect_lines=1))

    if current != 0:
        prompt = f"Leg address is currently {current}. Press Enter to keep it, or type a new address (1-6):"
    else:
        prompt = "This board has no leg address yet (uncalibrated). Enter the leg address to assign (1-6):"

    while True:
        s = input(prompt + " ").strip()
        if s == "":
            if current != 0:
                return current
            print("An address is required for an uncalibrated board.")
            continue
        try:
            n = int(s)
        except ValueError:
            print("Enter a whole number 1-6.")
            continue
        if not (1 <= n <= 6):
            print("Must be 1-6.")
            continue
        if n == current:
            return current
        resp = link.command(f"ADDR {n}", expect_lines=1)
        if resp and resp[0].startswith("OK"):
            print(f"OK: {resp[0]}")
            return n
        print(f"ERR unexpected response: {resp}")


def calibrate_channel(link: LegLink, name: str, ch: int) -> None:
    connector = CONNECTOR[name]
    print(f"\n--- {name} current calibration (channel {ch}, connector {connector}) ---")
    raw = []
    ref = []

    wait_enter(f"Disconnect the servo from {connector} ({name}, open circuit, no load), then press Enter.")
    r0 = read_raw_mv(link, ch)
    print(f"  zero-load raw = {r0:.3f} mV")
    raw.append(r0)
    ref.append(0.0)

    for ohms, power in zip(SPAN_OHMS, SPAN_POWER):
        wait_enter(f"Attach a {ohms} ohm resistor (rated >= {power}) to {connector} ({name}), then press Enter.")
        i_ref = read_float("Enter the measured/computed reference current for this load, in mA:")
        r = read_raw_mv(link, ch)
        print(f"  raw = {r:.3f} mV")
        raw.append(r)
        ref.append(i_ref)

    try:
        scale, offset = fit_line(raw, ref)
    except RuntimeError as e:
        print(f"ERR {e}, skipping this channel")
        return
    print(f"Computed scale={scale:.6f} offset={offset:.3f} (least squares over {len(raw)} points)")

    resp = link.command(f"CURCAL {ch} {scale:.6f} {offset:.3f}", expect_lines=1)
    if resp and resp[0].startswith("OK"):
        print(f"OK written and persisted: {resp[0]}")
    else:
        print(f"ERR unexpected response: {resp}")


def run_zero_position(link: LegLink) -> None:
    print(f"Centering all three servos to {NEUTRAL_PWM_US} us for assembly...")
    for joint in range(3):
        resp = link.command(f"PWM {joint} {NEUTRAL_PWM_US}", expect_lines=1)
        print(f"  joint {joint}: {resp[0] if resp else '(no response)'}")
    print("Done -- these are raw overrides, not persisted; power-cycle or send a real")
    print("target (e.g. from the mainboard) to release them.")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", required=True, help="serial port, e.g. COM10")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--channels", default="coxa,femur,tibia",
                     help="comma-separated subset of coxa,femur,tibia (default: all three)")
    ap.add_argument("--zero", action="store_true",
                     help="build aid: center all 3 servos and exit (skips current calibration)")
    args = ap.parse_args()

    names = [n.strip() for n in args.channels.split(",") if n.strip()]
    for n in names:
        if n not in CHANNELS:
            print(f"Unknown channel '{n}', must be one of {list(CHANNELS)}", file=sys.stderr)
            sys.exit(1)

    link = LegLink(args.port, args.baud)

    ping = link.command("PING", expect_lines=1)
    print(f"Connected: {ping[0] if ping else '(no response)'}")

    print(BOARD_DIAGRAM)

    addr = prompt_leg_address(link)
    print(f"\nConfiguring leg {addr}.")

    if args.zero:
        run_zero_position(link)
        return

    for n in names:
        calibrate_channel(link, n, CHANNELS[n])

    print("\nDone.")


if __name__ == "__main__":
    main()
