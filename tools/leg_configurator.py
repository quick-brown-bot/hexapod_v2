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
  3. Zero-load offsets, all 4 channels at once: disconnect all three servos
     and one CURRAW? poll (averaged over FINAL_SAMPLE_COUNT samples) reports
     every channel's offset error in one step -- no separate "disconnect for
     total" pass, since a single poll already covers total/coxa/femur/tibia.
     Offset matters far more than scale here (the INA4181 gain + shunt put
     scale theoretically near 1.0, and it lands ~0.97-0.99 in practice), so
     this alone is enough for a reasonable calibration.
  4. Choose how far to take it:
     - Enter/'n' (default): write scale=1.0 with the just-measured offset to
       all 4 channels and stop there.
     - a number 0-2: use that as the scale for all 4 channels (still with
       their own measured offsets) -- e.g. if you already know this batch of
       boards runs close to 0.98, skip the resistor dance entirely.
     - 'y': go on to full per-channel resistor calibration (below), reusing
       the zero point already measured instead of re-measuring it.
  5. Resistor calibration (only if 'y' above), for each of coxa/femur/tibia
     in turn: attach each of a few known THT resistor loads. The reading is
     shown live (updated in place) and the load is auto-detected once it
     deviates from the zero-point baseline and settles -- no confirmation
     keypress needed for that part. Once settled, the recorded value is an
     average of FINAL_SAMPLE_COUNT fresh individual readings, not just
     whichever single poll tripped the detector. Enter the reference current
     (mA) for each; the prompt defaults to whatever you typed for that
     resistor value last time (or a first-run theoretical guess), so repeat
     runs are mostly Enter-Enter-Enter. At any point: 'b' redoes the
     previous reading step, 'r' re-measures the current one (both also work
     as a bare keypress while a reading is live, on Windows). Fits
     `current_ma = scale * raw_mv + offset` by ordinary least squares over
     the zero point + all span points, then persists it with
     `CURCAL <ch> <scale> <offset>`. `total` (channel 0) is then fit from
     points piggybacked for free during these branch calibrations -- CURRAW?
     already reports all 4 channels on every poll, so every span measurement
     doubles as a `total` data point (paired with the same reference
     current), no extra round trips. Valid only if the other two branches
     stay disconnected for the *whole* session, not just their own step --
     the wizard prints a reminder before starting.

The suggested resistor values stay in the tens-of-mA range on purpose -- the
INA4181 current-shunt amplifier is linear by design, so a handful of
low-power points is enough; there's no need (or safe simple way) to source
amps through a small THT resistor. See docs/development/LEG_CALIBRATION.md.

Usage:
    python tools/leg_configurator.py                          # auto-detects the port
    python tools/leg_configurator.py --port COM10              # or name it explicitly
    python tools/leg_configurator.py --channels coxa,femur
    python tools/leg_configurator.py --zero                    # build aid: center all 3 servos

Requires pyserial -- if you don't have it in your default Python, run this
with PlatformIO's bundled interpreter instead:
    ~/.platformio/penv/Scripts/python.exe tools/leg_configurator.py
"""
import argparse
import json
import sys
import time
from pathlib import Path

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
        "  ~/.platformio/penv/Scripts/python.exe tools/leg_configurator.py --port COM10",
        file=sys.stderr,
    )
    sys.exit(1)

# USB VID of the RP2040's built-in CDC bootrom/stdio (Raspberry Pi
# Foundation) -- what a XIAO RP2040 (LegBoard) enumerates as. Used to filter
# out unrelated ports (Bluetooth virtual COM ports, etc.) when auto-detecting.
RP2040_USB_VID = 0x2E8A

# calib.cpp channel indices (ADC_CH_TOTAL/COXA/FEMUR/TIBIA order).
CHANNELS = {"coxa": 1, "femur": 2, "tibia": 3}
ALL_CHANNEL_NAMES = {0: "total", 1: "coxa", 2: "femur", 3: "tibia"}
# Servo connector per channel, from hardware/legboard/legboard_sch.py
# (J2/COXA_PWM, J3/FEMUR_PWM, J4/TIBIA_PWM) -- see the board diagram below.
CONNECTOR = {"coxa": "J2", "femur": "J3", "tibia": "J4"}
NEUTRAL_PWM_US = 1500  # DEFAULT_PWM_NEUTRAL_US, firmware/leg/src/config.h

# Common E12 THT resistor values, low-to-high current, with the minimum
# power rating each needs at a ~6V servo rail (kept small on purpose).
SPAN_OHMS = [470, 220, 100, 47]
SPAN_POWER = ["1/4W", "1/4W (1/2W recommended)", "1/2W", "1W"]
NOMINAL_RAIL_V = 6.0  # servo rail, only used for a first-run reference-current guess

# Remembers the reference current you typed for each resistor value last
# time, so re-runs (e.g. calibrating the next leg) default to that instead of
# a plain theoretical guess. Lives outside the repo -- this is a per-operator
# convenience, not project state.
CACHE_PATH = Path.home() / ".hexapod_leg_configurator.json"

# Live-detection tuning for wait_for_load_change(): a reading counts as
# "load attached" once it has moved at least this far from the zero-load
# baseline and held roughly steady (within STABLE_JITTER_MV) for
# STABLE_SAMPLES consecutive polls -- filters out both ADC noise sitting at
# the baseline and the bounce while you're still touching the leads together.
LOAD_DETECT_THRESHOLD_MV = 3.0
STABLE_JITTER_MV = 0.5
STABLE_SAMPLES = 3
POLL_INTERVAL_S = 0.15
LOAD_DETECT_TIMEOUT_S = 90.0

# Once the live-detect loop above decides the reading has settled, the
# actual recorded measurement is the average of this many fresh individual
# samples (rather than just whatever single noisy poll happened to trip the
# detector) -- same idea for the zero point.
FINAL_SAMPLE_COUNT = 20
FINAL_SAMPLE_INTERVAL_S = 0.05

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


def autodetect_port() -> str:
    """Pick a serial port without the user having to type one.

    Prefers ports enumerating as the RP2040's USB VID (filters out unrelated
    ports like Windows' Bluetooth virtual COM ports); falls back to any port
    if none match. Auto-picks if there's exactly one candidate, otherwise
    prompts.
    """
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


def read_all_raw_mv(link: LegLink) -> dict:
    """{channel_index: raw_mv} for all 4 channels, from one CURRAW? call."""
    lines = link.command("CURRAW?", expect_lines=4)
    out = {}
    for line in lines:
        if not line.startswith("CH"):
            continue
        toks = line.split()
        try:
            ch = int(toks[0][2:])
        except ValueError:
            continue
        for t in toks:
            if t.startswith("mv="):
                out[ch] = float(t[len("mv="):])
    return out


def read_raw_mv(link: LegLink, ch: int) -> float:
    all_mv = read_all_raw_mv(link)
    if ch not in all_mv:
        raise RuntimeError(f"CURRAW? did not report channel {ch}: {all_mv}")
    return all_mv[ch]


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


def sample_averaged(link: LegLink, ch: int, n: int = FINAL_SAMPLE_COUNT,
                     interval: float = FINAL_SAMPLE_INTERVAL_S, live: bool = True) -> dict:
    """Average `n` fresh CURRAW? polls across ALL 4 channels at once (same
    round trips a single-channel average would need anyway) -- steadier than
    trusting any single reading, and piggybacks a free `total` (channel 0)
    reading onto every branch measurement at no extra cost (see
    calibrate_channel()). Returns {channel_index: averaged_mv}. If `live`,
    prints progress for `ch` in place via '\\r'."""
    sums = {}
    counts = {}
    for k in range(n):
        for c, v in read_all_raw_mv(link).items():
            sums[c] = sums.get(c, 0.0) + v
            counts[c] = counts.get(c, 0) + 1
        if live:
            sys.stdout.write(f"\r  sampling {k + 1}/{n}...                                      ")
            sys.stdout.flush()
        if k < n - 1:
            time.sleep(interval)
    avg = {c: sums[c] / counts[c] for c in sums}
    if live:
        sys.stdout.write(f"\r  raw = {avg.get(ch, 0.0):.3f} mV (avg of {n})                                \n")
        sys.stdout.flush()
    return avg


BACK = object()  # sentinel: "redo the previous reading step"
REDO = object()  # sentinel: "re-measure this same step"


def wait_enter(prompt: str) -> None:
    input(prompt + " ")


def read_float_default(prompt: str, default: float):
    """Prompt for a float; Enter accepts `default`; 'b'/'r' return BACK/REDO."""
    while True:
        s = input(f"{prompt} [{default:.1f}, 'b'=back, 'r'=re-measure]: ").strip()
        if s == "":
            return default
        low = s.lower()
        if low in ("b", "back"):
            return BACK
        if low in ("r", "redo"):
            return REDO
        try:
            return float(s)
        except ValueError:
            print("Not a number (or 'b' to go back, 'r' to re-measure), try again.")


def load_ref_current_cache() -> dict:
    try:
        with open(CACHE_PATH, "r", encoding="utf-8") as f:
            return {float(k): float(v) for k, v in json.load(f).items()}
    except (FileNotFoundError, ValueError, json.JSONDecodeError, OSError):
        return {}


def save_ref_current(cache: dict, ohms: float, value: float) -> None:
    cache[ohms] = value
    try:
        with open(CACHE_PATH, "w", encoding="utf-8") as f:
            json.dump({str(k): v for k, v in cache.items()}, f, indent=2)
    except OSError:
        pass  # best-effort; a stale/missing cache just falls back to theory


def default_ref_current_ma(cache: dict, ohms: float) -> float:
    if ohms in cache:
        return cache[ohms]
    return 1000.0 * NOMINAL_RAIL_V / ohms  # I = V / R, in mA


def _poll_keypress():
    """Non-blocking single-keypress poll (Windows only). Returns 'enter',
    'back' (b/B), 'redo' (r/R), or None if nothing new was pressed since the
    last call."""
    if msvcrt is None:
        return None
    result = None
    while msvcrt.kbhit():
        c = msvcrt.getwch().lower()
        if c in ("\r", "\n"):
            result = "enter"
        elif c == "b":
            result = "back"
        elif c == "r":
            result = "redo"
    return result


def wait_for_load_change(link: LegLink, ch: int, baseline_mv: float, prompt: str):
    """Print `prompt`, then live-update the raw reading on one line (via
    '\\r') until it has moved away from `baseline_mv` and settled -- i.e. a
    resistor got attached. The returned value is then a {channel: avg_mv}
    dict (see sample_averaged()) -- the average of FINAL_SAMPLE_COUNT fresh
    individual readings across all 4 channels, not just whatever single poll
    happened to trip the detector.

    On Windows: pressing Enter at any point forces an immediate reading
    instead of waiting for auto-detection (e.g. if the change is too small
    to trip the threshold); 'b' aborts and returns BACK to redo the previous
    step; 'r' restarts the detection/settle wait in place (e.g. you bumped a
    lead while it was mid-detect). Falls back to a fixed timeout on
    platforms without keypress polling.
    """
    print(prompt)
    hint = "Enter=read now, b=back, r=restart" if msvcrt is not None else f"auto/timeout {LOAD_DETECT_TIMEOUT_S:.0f}s"
    print(f"  (watching for a change from the {baseline_mv:.3f} mV baseline -- {hint})")

    stable_count = 0
    last = baseline_mv
    deadline = time.time() + LOAD_DETECT_TIMEOUT_S

    while True:
        key = _poll_keypress()
        if key == "back":
            sys.stdout.write("\r  (going back)                                        \n")
            sys.stdout.flush()
            return BACK
        if key == "redo":
            sys.stdout.write("\r  (restarting)                                        \n")
            sys.stdout.flush()
            stable_count = 0
            last = baseline_mv
            deadline = time.time() + LOAD_DETECT_TIMEOUT_S
            continue
        if key == "enter":
            return sample_averaged(link, ch)

        r = read_raw_mv(link, ch)
        sys.stdout.write(f"\r  raw = {r:.3f} mV                                      ")
        sys.stdout.flush()

        if abs(r - baseline_mv) >= LOAD_DETECT_THRESHOLD_MV:
            stable_count = stable_count + 1 if abs(r - last) < STABLE_JITTER_MV else 0
            if stable_count >= STABLE_SAMPLES:
                return sample_averaged(link, ch)
        else:
            stable_count = 0
        last = r

        if time.time() > deadline:
            print()  # move off the live-update line before the warning
            print("  no change detected within the timeout, measuring anyway:")
            return sample_averaged(link, ch)

        time.sleep(POLL_INTERVAL_S)


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


TOTAL_CH = 0


def measure_all_offsets(link: LegLink) -> dict:
    """The one, shared zero-load measurement: with nothing connected to any
    of the three servo channels, current_ma should read 0 everywhere, so
    whatever raw mV each channel reports *is* that channel's offset error.
    One CURRAW? poll already reports all 4 channels, so this covers total
    too in the same step -- no separate "disconnect for total" pass needed.
    """
    wait_enter("Disconnect all three servos (coxa/femur/tibia -- open circuit, no load), then press Enter.")
    avg = sample_averaged(link, TOTAL_CH)
    print("Zero-load raw readings (this channel's offset error):")
    for ch, name in ALL_CHANNEL_NAMES.items():
        print(f"  CH{ch} {name}: {avg.get(ch, 0.0):.3f} mV")
    return avg


def prompt_calibration_mode():
    """Returns ("offsets", 1.0), ("scale", <0-2>), or ("resistors", None)."""
    s = input(
        "\nMore precise calibration with resistors? [y/N], or enter a scale "
        "(0-2) to use for every channel together with the offsets above: "
    ).strip()
    if s == "" or s.lower() in ("n", "no"):
        return "offsets", 1.0
    if s.lower() in ("y", "yes"):
        return "resistors", None
    try:
        val = float(s)
    except ValueError:
        print("Not understood -- defaulting to offsets-only (scale=1.0).")
        return "offsets", 1.0
    if 0.0 <= val <= 2.0:
        return "scale", val
    print("Scale out of range (0-2) -- defaulting to offsets-only (scale=1.0).")
    return "offsets", 1.0


def push_offset_only(link: LegLink, ch: int, scale: float, zero_mv: float) -> None:
    """Persist `scale` with whatever offset makes this channel read exactly
    0 mA at the already-measured zero-load raw reading:
    0 = zero_mv * scale + offset  =>  offset = -scale * zero_mv."""
    offset = -scale * zero_mv
    resp = link.command(f"CURCAL {ch} {scale:.6f} {offset:.3f}", expect_lines=1)
    if resp and resp[0].startswith("OK"):
        print(f"OK CH{ch} {ALL_CHANNEL_NAMES[ch]}: {resp[0]}")
    else:
        print(f"ERR CH{ch} {ALL_CHANNEL_NAMES[ch]}: unexpected response {resp}")


def calibrate_channel(link: LegLink, name: str, ch: int, ref_cache: dict,
                       total_raw: list, total_ref: list, zero_mv: dict) -> None:
    """Resistor-span calibration for one branch channel, starting from the
    zero point already measured by measure_all_offsets() (`zero_mv`) --
    no separate per-channel disconnect/re-measure. Also appends this
    channel's raw `total` (channel 0) reading at every span point to
    `total_raw`/`total_ref` (paired with the same reference current) --
    CURRAW? already reports all 4 channels per poll, so this piggybacks a
    free calibration data set for `total` onto the branch measurements at no
    extra cost. Valid only if the other two branches stay disconnected for
    the whole session, not just their own step (see the note printed in
    main())."""
    connector = CONNECTOR[name]
    print(f"\n--- {name} current calibration (channel {ch}, connector {connector}) ---")
    print(f"  using pre-measured zero-load raw = {zero_mv[ch]:.3f} mV")
    print("(At any prompt: 'b' redoes the previous step, 'r' re-measures this one.)")

    # points[0] is the shared zero point (already measured); steps 1..len(SPAN_OHMS)
    # are the resistor loads. Index-driven (not a for loop) so `back`/`redo` can
    # rewind it; 1 is as far back as it goes (no per-channel zero to redo here).
    n_steps = 1 + len(SPAN_OHMS)
    points = [None] * n_steps  # (raw_mv, ref_ma), filled in as we go
    points[0] = (zero_mv[ch], 0.0)
    i = 1
    while i < n_steps:
        ohms = SPAN_OHMS[i - 1]
        power = SPAN_POWER[i - 1]
        baseline = points[0][0]

        prompt = f"Attach a {ohms} ohm resistor (rated >= {power}) to {connector} ({name})."
        avg = wait_for_load_change(link, ch, baseline, prompt)
        if avg is BACK:
            i = max(1, i - 1)
            continue

        default = default_ref_current_ma(ref_cache, ohms)
        i_ref = read_float_default("Enter the measured/computed reference current for this load, in mA", default)
        if i_ref is BACK:
            i = max(1, i - 1)
            continue
        if i_ref is REDO:
            continue  # same step: re-measure the raw reading, keep the index

        save_ref_current(ref_cache, ohms, i_ref)
        points[i] = (avg[ch], i_ref)
        total_raw.append(avg[TOTAL_CH])
        total_ref.append(i_ref)
        i += 1

    raw = [p[0] for p in points]
    ref = [p[1] for p in points]

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


def calibrate_total(link: LegLink, total_raw: list, total_ref: list) -> None:
    """Fit and persist the `total` channel from points piggybacked during
    the branch calibrations (see calibrate_channel())."""
    print(f"\n--- total current calibration (channel {TOTAL_CH}) ---")
    print(f"Using {len(total_raw)} points collected as a byproduct of the branch")
    print("calibration above (only valid if the other branches stayed disconnected")
    print("for the whole session, not just their own step).")

    try:
        scale, offset = fit_line(total_raw, total_ref)
    except RuntimeError as e:
        print(f"ERR {e}, skipping total")
        return
    print(f"Computed scale={scale:.6f} offset={offset:.3f} (least squares over {len(total_raw)} points)")

    resp = link.command(f"CURCAL {TOTAL_CH} {scale:.6f} {offset:.3f}", expect_lines=1)
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
    ap.add_argument("--port", help="serial port, e.g. COM10 (auto-detected if omitted)")
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

    port = args.port or autodetect_port()
    link = LegLink(port, args.baud)

    ping = link.command("PING", expect_lines=1)
    print(f"Connected: {ping[0] if ping else '(no response)'}")

    print(BOARD_DIAGRAM)

    addr = prompt_leg_address(link)
    print(f"\nConfiguring leg {addr}.")

    if args.zero:
        run_zero_position(link)
        return

    print()
    zero_mv = measure_all_offsets(link)

    mode, scale = prompt_calibration_mode()

    if mode != "resistors":
        print(f"\nWriting scale={scale:.6f} with the measured offsets to all 4 channels...")
        for ch in ALL_CHANNEL_NAMES:
            push_offset_only(link, ch, scale, zero_mv.get(ch, 0.0))
        print("\nDone.")
        return

    print("\nTip: for a bonus 'total' channel calibration piggybacked for free on the")
    print("branch measurements below, keep ALL THREE servos disconnected for this whole")
    print("session -- not just whichever one is being tested at the moment.")

    ref_cache = load_ref_current_cache()
    total_raw, total_ref = [zero_mv.get(TOTAL_CH, 0.0)], [0.0]
    for n in names:
        calibrate_channel(link, n, CHANNELS[n], ref_cache, total_raw, total_ref, zero_mv)

    if len(total_raw) >= 2:
        calibrate_total(link, total_raw, total_ref)
    else:
        print("\nSkipping total: not enough points collected for a fit.")

    print("\nDone.")


if __name__ == "__main__":
    main()
