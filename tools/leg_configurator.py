#!/usr/bin/env python3
"""Interactive LegBoard configurator, driven directly over the board's own
USB serial console (firmware/leg/src/calib.cpp) -- no separate calibration
board or RS485 link needed.

Modes (--mode):
  full     (default) leg address -> current-sense calibration -> servo
           calibration, in sequence.
  address  Just assign/confirm the leg's RS485 address (`ADDR`), nothing else.
  current  Current-sense calibration only (see below).
  servo    Servo calibration only: PWM center (neutral), a gentle direction
           check, and an optional extended range walk (see below).
  invert   Query or directly set/flip one or more joints' persisted `INVERT`
           sign (--channels selects which), without the centering + gentle
           direction check + optional range walk that `--mode servo` runs
           for every selected joint. Use this when you already know a
           joint's sign is backwards (e.g. from a `--mode servo` run on a
           different leg of the same build) and just want to fix this one
           joint on this board. With neither --invert nor --toggle-invert,
           just reports the current persisted value(s).
  center   Just the live-jog PWM-center (neutral) step for one or more
           joints (--channels selects which) -- no gentle direction check or
           extended range walk. Use this to touch up a joint's neutral
           (e.g. it drifted, or the horn was re-seated) without re-running
           the direction check for joints that are already known-good.

Every mode except 'address' shows the board diagram and confirms/sets the
leg address first, since both current-sense and servo work need you to know
which physical connector is which.

## Current-sense calibration (--mode current, and part of full)

  1. Zero-load offsets, all 4 channels at once: disconnect all three servos
     and one CURRAW? poll (averaged over FINAL_SAMPLE_COUNT samples) reports
     every channel's offset error in one step -- no separate "disconnect for
     total" pass, since a single poll already covers total/coxa/femur/tibia.
     Offset matters far more than scale here (the INA4181 gain + shunt put
     scale theoretically near 1.0, and it lands ~0.97-0.99 in practice), so
     this alone is enough for a reasonable calibration.
  2. Choose how far to take it:
     - Enter/'n' (default): write scale=1.0 with the just-measured offset to
       all 4 channels and stop there.
     - a number 0-2: use that as the scale for all 4 channels (still with
       their own measured offsets) -- e.g. if you already know this batch of
       boards runs close to 0.98, skip the resistor dance entirely.
     - 'y': go on to full per-channel resistor calibration (below), reusing
       the zero point already measured instead of re-measuring it.
  3. Resistor calibration (only if 'y' above), for each of coxa/femur/tibia
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

  The suggested resistor values stay in the tens-of-mA range on purpose --
  the INA4181 current-shunt amplifier is linear by design, so a handful of
  low-power points is enough; there's no need (or safe simple way) to source
  amps through a small THT resistor. See docs/development/LEG_CALIBRATION.md.

  The very first step (disconnect all three servos, then Enter to take the
  zero-load reading) can be skipped with Esc instead of Enter -- skips
  current-sense calibration entirely for this run and leaves whatever is
  already stored on the board untouched, e.g. if you only came to check/
  adjust servo centers.

## Servo calibration (--mode servo, and part of full)

  For each selected joint, in turn:
  1. Center: a live jog UI -- UP/DOWN nudges the pulse width by
     JOG_STEP_FINE_US, LEFT/RIGHT by the coarser JOG_STEP_COARSE_US, the
     leg moves and the current value redraws in place as you go. Enter
     accepts and persists that pulse as the joint's PWM neutral
     (`PWMNEUTRAL`) -- compensates for a leg that can't be mounted with
     exactly 1500us at its true physical center (a servo horn spline is
     rarely a perfect fit). Esc cancels, leaving the previous center
     untouched. Takes effect immediately -- also what `--zero` centers to
     afterward, and what a commanded angle of 0 degrees maps to in normal
     (RS485-driven) operation.
  2. Gentle direction check: a small nudge (`--step` us, default 100) to
     each side of that just-set center -- catches a backwards joint before
     doing anything more. Phrased entirely in physical-motion terms (per
     JOINT_EXPECTED_DIRECTION -- coxa: counter-clockwise/clockwise viewed
     from above with Z up; femur: up/down; tibia: curl under/straighten --
     see docs/architecture/HARDWARE_AND_MECHANICS.md "Joint Angle Sign
     Convention" for the derivation), not PWM pulse widths or INVERT signs:
     states the expected direction, nudges, and asks whether it moved that
     way -- Enter/'y' accepts and continues, 'r' retries the same nudge,
     'i' flips+persists `INVERT` and retries, so a wrong direction is fixed
     and reconfirmed immediately rather than found out during a later
     IK/gait test. Positive is tested first, then negative the same way.
     Current is sampled at every nudge against a just-measured neutral
     baseline, as a stall/binding safety net (informational only, not a
     hard cutoff). Returns to neutral and moves on to the next joint once
     both directions are confirmed, with no further confirmation needed.
  3. Optional, asked once after all selected joints' gentle checks: an
     extended range walk per joint toward the min/max pulse in the same
     small increments, still step-confirmed and current-monitored, to find
     the safe usable range without jamming anything.

  All joints return to their (possibly just-recalibrated) center at the end,
  or immediately if you quit early with 'q'. Angle range
  (`angle_min_deg`/`angle_max_deg`) is not yet calibratable this way -- still
  a compile-time default, see `firmware/leg/README.md` "Bring-Up Status/TODO".

Usage:
    python tools/leg_configurator.py                          # full wizard, auto-detects the port
    python tools/leg_configurator.py --port COM10
    python tools/leg_configurator.py --mode address             # just set the leg number
    python tools/leg_configurator.py --mode current              # current-sense calibration only
    python tools/leg_configurator.py --mode current --channels coxa
    python tools/leg_configurator.py --mode servo                # servo center/direction/range only
    python tools/leg_configurator.py --mode servo --channels femur --step 150
    python tools/leg_configurator.py --mode center --channels femur   # re-center just femur's neutral
    python tools/leg_configurator.py --mode invert --channels tibia   # report tibia's current INVERT
    python tools/leg_configurator.py --mode invert --channels tibia --invert -1
    python tools/leg_configurator.py --mode invert --channels coxa --toggle-invert
    python tools/leg_configurator.py --zero                      # build aid: center all 3 servos

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

# calib.cpp channel indices (ADC_CH_TOTAL/COXA/FEMUR/TIBIA order). Note these
# are current-sense channel indices, NOT the same numbering as the PWM joint
# indices below (JOINTS) -- different namespaces that happen to overlap.
CHANNELS = {"coxa": 1, "femur": 2, "tibia": 3}
ALL_CHANNEL_NAMES = {0: "total", 1: "coxa", 2: "femur", 3: "tibia"}
# Servo connector per channel, from hardware/legboard/legboard_sch.py
# (J2/COXA_PWM, J3/FEMUR_PWM, J4/TIBIA_PWM) -- see the board diagram below.
CONNECTOR = {"coxa": "J2", "femur": "J3", "tibia": "J4"}
NEUTRAL_PWM_US = 1500  # DEFAULT_PWM_NEUTRAL_US, firmware/leg/src/config.h -- only the
                       # factory fallback; the actual per-joint center may have been
                       # recalibrated, see query_pwm_neutral() / --mode servo.
PWM_MIN_US = 500       # DEFAULT_PWM_MIN_US
PWM_MAX_US = 2500      # DEFAULT_PWM_MAX_US

# PWM joint indices (calib.cpp's PWM/PWMNEUTRAL commands, config.h JOINT_COXA/
# FEMUR/TIBIA) -- in build order, distinct from the current-sense CHANNELS above.
JOINTS = [("coxa", 0), ("femur", 1), ("tibia", 2)]

# --mode servo: purely informational -- flags a current jump worth a second
# look while jogging/testing a joint, not a hard safety cutoff (the board may
# not be current-calibrated yet, so this compares raw mV against the
# just-measured neutral baseline, not amps).
CURRENT_WARN_DELTA_MV = 15.0

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


# --- Raw single-keypress reading (servo jog UI, disconnect-servos skip) ---
#
# Used where a full Enter-terminated line is the wrong interaction: live
# arrow-key jogging (interactive_center_joint()) and a plain Enter-vs-Esc
# choice (wait_confirm_or_skip()). Normalizes to 'UP'/'DOWN'/'LEFT'/'RIGHT'/
# 'ENTER'/'ESC'/'Q', or a single printable character otherwise.
#
# POSIX: puts the terminal in cbreak mode (termios/tty) for the duration and
# reads raw bytes, disambiguating a lone Esc from the start of an arrow-key
# escape sequence (ESC '[' 'A'/'B'/'C'/'D') with a short non-blocking peek
# (arrow-key bytes arrive together; a real Esc press has nothing following
# it). Windows: msvcrt.getwch(), which already returns arrow keys as a
# two-call prefix+code pair, no raw-mode switch needed. Neither path works
# without a real interactive terminal (e.g. piped/redirected stdin) -- falls
# back to line input there, treating a blank line as ENTER and 'esc'/'skip'
# as ESC.

def _posix_read_key(fd) -> str:
    # Deliberately os.read() on the raw fd, not sys.stdin.read(): a buffered
    # TextIOWrapper read can slurp more bytes than requested into its own
    # userspace buffer in one syscall, which would make the select() peeks
    # below (which only see what's still sitting in the kernel) wrongly
    # report "nothing pending" for bytes Python already grabbed -- turning
    # every arrow-key sequence into a false lone-Esc.
    import os
    import select
    ch = os.read(fd, 1).decode(errors="replace")
    if ch == "\x1b":
        if select.select([fd], [], [], 0.02)[0]:
            ch2 = os.read(fd, 1).decode(errors="replace")
            if ch2 == "[" and select.select([fd], [], [], 0.02)[0]:
                ch3 = os.read(fd, 1).decode(errors="replace")
                return {"A": "UP", "B": "DOWN", "C": "RIGHT", "D": "LEFT"}.get(ch3, "ESC")
            return "ESC"
        return "ESC"
    if ch in ("\r", "\n"):
        return "ENTER"
    return ch


def _win_read_key() -> str:
    c = msvcrt.getwch()
    if c in ("\x00", "\xe0"):  # arrow/function-key prefix
        c2 = msvcrt.getwch()
        return {"H": "UP", "P": "DOWN", "K": "LEFT", "M": "RIGHT"}.get(c2, "")
    if c == "\x1b":
        return "ESC"
    if c in ("\r", "\n"):
        return "ENTER"
    return c


class RawKeys:
    """Context manager + single-keypress reader; see module note above."""

    def __init__(self):
        self._posix = sys.platform != "win32" and sys.stdin.isatty()
        self._interactive = self._posix or msvcrt is not None

    def __enter__(self):
        if self._posix:
            import termios
            import tty
            self._termios = termios
            self._fd = sys.stdin.fileno()
            self._old = termios.tcgetattr(self._fd)
            tty.setcbreak(self._fd)
        return self

    def __exit__(self, exc_type, exc, tb):
        if self._posix:
            self._termios.tcsetattr(self._fd, self._termios.TCSADRAIN, self._old)
        return False

    def read(self) -> str:
        if self._posix:
            key = _posix_read_key(self._fd)
        elif msvcrt is not None:
            key = _win_read_key()
        else:
            # No raw keyboard available (e.g. non-interactive stdin) -- fall
            # back to line input; arrow-key jogging degrades to typing an
            # exact value followed by Enter.
            s = input().strip()
            if s == "":
                return "ENTER"
            if s.lower() in ("esc", "escape", "skip"):
                return "ESC"
            return s
        return "Q" if key in ("q", "Q") else key


def wait_confirm_or_skip(prompt: str) -> bool:
    """Print `prompt`, then wait for a single keypress: Enter confirms and
    proceeds (True), Esc (or 'q') skips (False)."""
    print(f"{prompt} [Enter=confirm, Esc=skip] ", end="", flush=True)
    with RawKeys() as keys:
        while True:
            key = keys.read()
            if key == "ENTER":
                print()
                return True
            if key in ("ESC", "Q"):
                print("skipped.")
                return False


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


def measure_all_offsets(link: LegLink):
    """The one, shared zero-load measurement: with nothing connected to any
    of the three servo channels, current_ma should read 0 everywhere, so
    whatever raw mV each channel reports *is* that channel's offset error.
    One CURRAW? poll already reports all 4 channels, so this covers total
    too in the same step -- no separate "disconnect for total" pass needed.

    Returns the averaged {channel: mv} dict, or None if skipped (Esc) --
    the caller then skips current-sense calibration entirely for this run.
    """
    if not wait_confirm_or_skip("Disconnect all three servos (coxa/femur/tibia -- open circuit, no load), then press Enter."):
        return None
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


def query_pwm_neutral(link: LegLink) -> dict:
    """{joint_index: persisted PWM neutral (center) us} from PWMNEUTRAL?."""
    lines = link.command("PWMNEUTRAL?", expect_lines=3)
    out = {}
    for line in lines:
        if not line.startswith("J"):
            continue
        toks = line.split()
        try:
            j = int(toks[0][1:])
        except ValueError:
            continue
        for t in toks:
            if t.startswith("neutral_us="):
                out[j] = int(t[len("neutral_us="):])
    return out


def query_invert(link: LegLink) -> dict:
    """{joint_index: persisted sign (+1/-1)} from INVERT?."""
    lines = link.command("INVERT?", expect_lines=3)
    out = {}
    for line in lines:
        if not line.startswith("J"):
            continue
        toks = line.split()
        try:
            j = int(toks[0][1:])
        except ValueError:
            continue
        for t in toks:
            if t.startswith("invert="):
                out[j] = int(t[len("invert="):])
    return out


def persist_invert(link: LegLink, joint: int, invert: int) -> bool:
    resp = link.command(f"INVERT {joint} {invert}", expect_lines=1)
    ok = bool(resp) and resp[0].startswith("OK")
    print(f"    -> {resp[0] if resp else '(no response)'}")
    return ok


# Short, physical-motion phrasing for the gentle direction check -- (word
# for a positive commanded angle, word for negative), deliberately free of
# PWM/INVERT jargon since the check is about what the leg visibly does, not
# about the signal driving it. See docs/architecture/HARDWARE_AND_MECHANICS.md
# "Joint Angle Sign Convention" for the full derivation (from
# hex_kinematics/leg.c) and hardware confirmation. Coxa's rotation direction
# is a geometric property of the leg-local frame itself, so it's universal
# across all six legs, even though what it means in *body*-frame terms
# ("left" vs "right") depends on each leg's mount rotation -- that
# mount-rotation dependency is about translating this into body-frame
# language, not about whether this leg-local expectation holds.
JOINT_EXPECTED_DIRECTION = {
    "coxa":  ("counter-clockwise (viewed from above, Z up)", "clockwise (viewed from above, Z up)"),
    "femur": ("up", "down"),
    "tibia": ("curl under", "straighten"),
}


def run_zero_position(link: LegLink) -> None:
    neutral = query_pwm_neutral(link)
    print("Centering all three servos to their calibrated PWM neutral for assembly...")
    for name, joint in JOINTS:
        us = neutral.get(joint, NEUTRAL_PWM_US)
        resp = link.command(f"PWM {joint} {us}", expect_lines=1)
        print(f"  {name} (joint {joint}): {us} us -- {resp[0] if resp else '(no response)'}")
    print("Done -- these are raw overrides, not persisted; power-cycle or send a real")
    print("target (e.g. from the mainboard) to release them.")


# --- Servo calibration (--mode servo): PWM center, direction, range -------

class Abort(Exception):
    """Raised when the user quits early ('q' at any --mode servo prompt)."""


def wizard_step(text: str) -> None:
    """Print an upcoming action and wait for the user to press Enter.
    Typing 'q' aborts the servo wizard (caught by the caller, which
    re-centers before returning)."""
    print(f"\n>>> {text}")
    ans = input("    [Enter to continue, q to quit] > ").strip().lower()
    if ans == "q":
        raise Abort()


def confirm(prompt: str, default_yes: bool = False) -> bool:
    suffix = "[Y/n]" if default_yes else "[y/N]"
    ans = input(f"{prompt} {suffix} > ").strip().lower()
    if ans == "":
        return default_yes
    return ans in ("y", "yes")


def send_pwm(link: LegLink, joint: int, us: int) -> None:
    resp = link.command(f"PWM {joint} {us}", expect_lines=1)
    print(f"    -> {resp[0] if resp else '(no response)'}")


def send_pwm_quiet(link: LegLink, joint: int, us: int) -> None:
    """Like send_pwm(), but silent -- for the live jog UI, which redraws its
    own single status line instead of printing a response per keypress."""
    link.command(f"PWM {joint} {us}", expect_lines=1)


def report_current(link: LegLink, ch: int, baseline_mv: dict) -> None:
    raw = read_all_raw_mv(link)
    mv = raw.get(ch, 0.0)
    base = baseline_mv.get(ch, 0.0)
    delta = mv - base
    flag = "  <-- WATCH THIS: jumped well above the neutral baseline" \
        if abs(delta) >= CURRENT_WARN_DELTA_MV else ""
    print(f"    current CH{ch}: {mv:7.2f} mV (baseline {base:7.2f} mV, delta {delta:+6.2f} mV){flag}")


def center_all_to_neutral(link: LegLink, neutral: dict) -> None:
    for name, joint in JOINTS:
        send_pwm(link, joint, neutral.get(joint, NEUTRAL_PWM_US))


JOG_STEP_FINE_US = 5     # UP/DOWN
JOG_STEP_COARSE_US = 25  # LEFT/RIGHT


def interactive_center_joint(link: LegLink, name: str, joint: int, us: int) -> int:
    """Live-jog `joint`'s raw PWM pulse (starting from `us`) with the arrow
    keys until it looks physically centered for this leg's build. UP/DOWN
    step by JOG_STEP_FINE_US, LEFT/RIGHT by the coarser JOG_STEP_COARSE_US;
    the pulse is sent and the status line redrawn on every step. Enter
    accepts and returns the new pulse width (caller persists it via
    PWMNEUTRAL); Esc cancels, restores, and returns the original `us`
    unchanged.

    Falls back to typed values (no arrow keys) if stdin isn't a real
    terminal -- see RawKeys."""
    original = us
    send_pwm_quiet(link, joint, us)
    print(f"\n--- {name} (joint {joint}) -- centering, starting at {us} us ---")

    def redraw():
        sys.stdout.write(
            f"\r  {us} us   "
            f"[UP/DOWN ±{JOG_STEP_FINE_US}us, LEFT/RIGHT ±{JOG_STEP_COARSE_US}us, "
            f"Enter=accept, Esc=cancel]      "
        )
        sys.stdout.flush()

    redraw()
    with RawKeys() as keys:
        while True:
            key = keys.read()
            if key == "ENTER":
                print()
                return us
            if key in ("ESC", "Q"):
                if us != original:
                    send_pwm_quiet(link, joint, original)
                print(f"\n  cancelled, keeping {original} us")
                return original
            delta = {
                "UP": JOG_STEP_FINE_US, "DOWN": -JOG_STEP_FINE_US,
                "RIGHT": JOG_STEP_COARSE_US, "LEFT": -JOG_STEP_COARSE_US,
            }.get(key)
            if delta is None:
                # Non-interactive fallback (RawKeys returns typed text here):
                # accept a directly-typed pulse width too.
                try:
                    us = max(PWM_MIN_US, min(PWM_MAX_US, int(key)))
                    send_pwm_quiet(link, joint, us)
                    redraw()
                except ValueError:
                    pass
                continue
            us = max(PWM_MIN_US, min(PWM_MAX_US, us + delta))
            send_pwm_quiet(link, joint, us)
            redraw()


def persist_neutral(link: LegLink, joint: int, us: int) -> None:
    resp = link.command(f"PWMNEUTRAL {joint} {us}", expect_lines=1)
    if resp and resp[0].startswith("OK"):
        print(f"OK persisted: {resp[0]}")
    else:
        print(f"ERR unexpected response: {resp}")


def gentle_direction_check(link: LegLink, name: str, joint: int, ch: int,
                            step_us: int, us_neutral: int, baseline_mv: dict) -> None:
    """Interactive direction check, phrased entirely in physical-motion terms
    (JOINT_EXPECTED_DIRECTION) rather than PWM pulse widths or INVERT signs
    -- the operator is just confirming which way the leg moved, not reading
    signal values. Assumes the joint is already sitting at `us_neutral`
    (true for its one caller, run_servo_calibration(), which just finished
    interactive_center_joint() there) -- no separate "setting to neutral"
    step, since that would just be re-sending the pulse it's already at:

      1. State the expected direction for a positive commanded angle
         (Enter to continue).
      2. Nudge positive and ask whether it moved that way -- Enter/'y'
         accepts and moves on to step 3, 'r' retries (loops back to 1),
         'i' flips+persists INVERT then retries (loops back to 1).
      3. State the expected direction for a negative commanded angle.
      4. Nudge negative and ask the same way -- 'r'/'i' both loop back to 3.
      5. Return to neutral and move on to the next joint -- no further
         confirmation.

    `PWM <joint> <us>` (servo_write_pulse_us() in servo.cpp) is a *raw*
    pulse-width write that bypasses INVERT entirely -- only the real
    angle-command path (servo_write_angle()) applies it. So each nudge here
    is signed by the joint's currently persisted invert, mirroring what
    servo_write_angle() would do for a small commanded angle -- otherwise
    flipping INVERT would send the exact same raw pulse and 'i' would never
    visibly change anything (a real bug in an earlier version of this
    check)."""
    pos_word, neg_word = JOINT_EXPECTED_DIRECTION.get(name, ("+", "-"))
    print(f"\n=== {name.upper()} (joint {joint}) -- gentle direction check ===")

    def nudge_and_ask(sign: int, word: str) -> None:
        while True:
            wizard_step(f"{name}: expect it to move {word}")
            invert = query_invert(link).get(joint, 1)
            send_pwm(link, joint, us_neutral + sign * invert * step_us)
            report_current(link, ch, baseline_mv)
            ans = input(f"    did it move {word}? [Y/r=retry/i=invert] > ").strip().lower()
            send_pwm(link, joint, us_neutral)
            report_current(link, ch, baseline_mv)
            if ans == "r":
                continue
            if ans == "i":
                new_invert = -invert
                print(f"    flipping INVERT: {invert} -> {new_invert}")
                persist_invert(link, joint, new_invert)
                continue
            return

    nudge_and_ask(+1, pos_word)
    nudge_and_ask(-1, neg_word)

    send_pwm(link, joint, us_neutral)
    report_current(link, ch, baseline_mv)


def extended_range_walk(link: LegLink, name: str, joint: int, ch: int,
                         step_us: int, us_neutral: int, baseline_mv: dict) -> None:
    print(f"\n=== {name.upper()} (joint {joint}) -- extended range walk ===")
    print("    Stepping toward the max pulse width first, then back through the")
    print("    center toward the min. Stop at the first sign of binding or a")
    print("    sustained current jump.")

    for target in range(us_neutral, PWM_MAX_US + 1, step_us):
        wizard_step(f"{name}: {target} us (toward max {PWM_MAX_US} us)")
        send_pwm(link, joint, target)
        report_current(link, ch, baseline_mv)

    wizard_step(f"{name}: back to center ({us_neutral} us)")
    send_pwm(link, joint, us_neutral)
    report_current(link, ch, baseline_mv)

    for target in range(us_neutral, PWM_MIN_US - 1, -step_us):
        wizard_step(f"{name}: {target} us (toward min {PWM_MIN_US} us)")
        send_pwm(link, joint, target)
        report_current(link, ch, baseline_mv)

    wizard_step(f"{name}: back to center ({us_neutral} us) before the next joint")
    send_pwm(link, joint, us_neutral)
    report_current(link, ch, baseline_mv)


def run_servo_calibration(link: LegLink, names, step_us: int) -> None:
    """Per selected joint: interactively recalibrate the PWM center, then a
    step-confirmed gentle direction check around that center, then an
    optional extended range walk -- see the module docstring's "Servo
    calibration" section for the full flow."""
    print("\n=== Servo calibration: PWM center, direction, and range ===")
    print("Make sure the leg is free to move, and watch it (and the current")
    print("readout below) throughout. Type 'q' at any prompt to stop early --")
    print("all joints return to center first.")

    joint_of = dict(JOINTS)
    try:
        neutral = query_pwm_neutral(link)
        wizard_step("Center all three servos to their current calibrated neutral")
        center_all_to_neutral(link, neutral)

        wizard_step("Look at the leg: does the neutral pose look right? "
                    "Sampling the current baseline next.")
        baseline_mv = sample_averaged(link, TOTAL_CH)
        print("Baseline current at neutral, unloaded:")
        for name, joint in JOINTS:
            ch = CHANNELS[name]
            print(f"    {name:>5} (CH{ch}): {baseline_mv.get(ch, 0.0):7.2f} mV")
        print(f"    total (CH{TOTAL_CH}): {baseline_mv.get(TOTAL_CH, 0.0):7.2f} mV")

        for name in names:
            joint = joint_of[name]
            ch = CHANNELS[name]
            us_before = neutral.get(joint, NEUTRAL_PWM_US)

            us = interactive_center_joint(link, name, joint, us_before)
            if us != us_before:
                persist_neutral(link, joint, us)
            neutral[joint] = us

            gentle_direction_check(link, name, joint, ch, step_us, us, baseline_mv)

        if confirm("\nGentle checks done. Run the extended range walk too?"):
            for name in names:
                joint = joint_of[name]
                ch = CHANNELS[name]
                extended_range_walk(link, name, joint, ch, step_us, neutral[joint], baseline_mv)

        wizard_step("All done -- return everything to center")
        center_all_to_neutral(link, neutral)
        print("\nServo calibration complete.")

    except Abort:
        print("\nQuit requested -- returning all servos to center before exiting.")
        center_all_to_neutral(link, query_pwm_neutral(link))


def run_center_only(link: LegLink, names) -> None:
    """Just the live-jog PWM-center step for each selected joint (see
    interactive_center_joint) -- no gentle direction check or range walk.
    All joints are moved to their currently-persisted neutral first, so the
    leg is in a known pose before jogging starts, same as
    run_servo_calibration's first step."""
    print("\n=== PWM neutral (center) adjustment only ===")
    print("Make sure the leg is free to move. Type 'q' at any prompt to stop early --")
    print("all joints return to center first.")

    joint_of = dict(JOINTS)
    try:
        neutral = query_pwm_neutral(link)
        wizard_step("Center all three servos to their current calibrated neutral")
        center_all_to_neutral(link, neutral)

        for name in names:
            joint = joint_of[name]
            us_before = neutral.get(joint, NEUTRAL_PWM_US)
            us = interactive_center_joint(link, name, joint, us_before)
            if us != us_before:
                persist_neutral(link, joint, us)
            neutral[joint] = us

        wizard_step("All done -- return everything to center")
        center_all_to_neutral(link, neutral)
        print("\nPWM neutral adjustment complete.")

    except Abort:
        print("\nQuit requested -- returning all servos to center before exiting.")
        center_all_to_neutral(link, query_pwm_neutral(link))


def run_invert_only(link: LegLink, names, set_value, toggle) -> None:
    """Query or directly set/flip each selected joint's persisted INVERT
    sign -- no PWM movement, no gentle direction check. With both
    `set_value` and `toggle` falsy, just reports the current value(s)."""
    joint_of = dict(JOINTS)
    current = query_invert(link)
    for name in names:
        joint = joint_of[name]
        before = current.get(joint, 1)
        if set_value is not None:
            new = set_value
        elif toggle:
            new = -before
        else:
            print(f"  {name} (joint {joint}): invert={before}")
            continue
        if new == before:
            print(f"  {name} (joint {joint}): invert already {new}, nothing to do")
            continue
        print(f"  {name} (joint {joint}): invert {before} -> {new}")
        persist_invert(link, joint, new)


def run_address_only(link: LegLink) -> None:
    addr = prompt_leg_address(link)
    print(f"\nLeg address set to {addr}.")


def run_current_calibration(link: LegLink, names) -> None:
    print()
    zero_mv = measure_all_offsets(link)
    if zero_mv is None:
        print("Skipped current-sense calibration.")
        return

    mode, scale = prompt_calibration_mode()

    if mode != "resistors":
        print(f"\nWriting scale={scale:.6f} with the measured offsets to all 4 channels...")
        for ch in ALL_CHANNEL_NAMES:
            push_offset_only(link, ch, scale, zero_mv.get(ch, 0.0))
        print("\nCurrent calibration done.")
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

    print("\nCurrent calibration done.")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", help="serial port, e.g. COM10 (auto-detected if omitted)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--mode", choices=["full", "address", "current", "servo", "center", "invert"],
                     default="full",
                     help="full (default): address + current-sense + servo calibration in "
                          "sequence; address: just set the leg number; current: current-sense "
                          "calibration only; servo: PWM center/direction/range calibration only; "
                          "center: PWM neutral (center) adjustment only, no direction check or "
                          "range walk; invert: query or directly set/flip a joint's INVERT sign, "
                          "no PWM movement")
    ap.add_argument("--channels", default="coxa,femur,tibia",
                     help="comma-separated subset of coxa,femur,tibia -- applies to current/servo/"
                          "center/invert work (default: all three)")
    ap.add_argument("--step", type=int, default=100,
                     help="servo mode: pulse-width nudge in us for the gentle direction check and "
                          "the extended range walk (default 100)")
    ap.add_argument("--zero", action="store_true",
                     help="build aid: center all 3 servos to their calibrated neutral and exit")
    invert_group = ap.add_mutually_exclusive_group()
    invert_group.add_argument("--invert", type=int, choices=[1, -1], default=None,
                               help="invert mode: set the selected joint(s) INVERT to this value")
    invert_group.add_argument("--toggle-invert", action="store_true",
                               help="invert mode: flip the selected joint(s) currently persisted "
                                    "INVERT sign")
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

    if args.zero:
        run_zero_position(link)
        return

    if args.mode == "address":
        run_address_only(link)
        return

    if args.mode == "invert":
        # No physical movement here (unlike center/servo/current), so skip
        # the board diagram -- just confirm which leg is about to change.
        addr = prompt_leg_address(link)
        print(f"\nConfiguring leg {addr}.")
        run_invert_only(link, names, args.invert, args.toggle_invert)
        return

    print(BOARD_DIAGRAM)
    addr = prompt_leg_address(link)
    print(f"\nConfiguring leg {addr}.")

    if args.mode == "current":
        run_current_calibration(link, names)
    elif args.mode == "servo":
        run_servo_calibration(link, names, args.step)
    elif args.mode == "center":
        run_center_only(link, names)
    else:  # full
        run_current_calibration(link, names)
        run_servo_calibration(link, names, args.step)


if __name__ == "__main__":
    main()
