#!/usr/bin/env python3
"""Current-sensor calibration: drives a LegBoard + calibration adapter board
(hardware/calboard/) together, fits per-channel current calibration, and
pushes the result onto the LegBoard's flash.

Hardware setup: connect the calboard's 3 servo-style headers to a single
LegBoard's J2/J3/J4 in place of real servos, and plug both boards into this
PC over USB. See firmware/leg/src/calib.cpp (PWM/CURRAW?/CURCAL commands)
and firmware/calboard/src/usbserial.cpp (READ? command) for the wire
protocol this script drives.

Usage:
    pip install -r requirements.txt
    python calibrate_current.py                    # auto-detect ports
    python calibrate_current.py --leg COM5 --cal COM7
"""

from __future__ import annotations

import argparse
import csv
import datetime
import sys
import time
from dataclasses import dataclass, field

import serial
import serial.tools.list_ports

BAUD = 115200
SETTLE_S = 0.15
LADDER_STEPS = 15
PWM_MIN_US = 1000
PWM_MAX_US = 2000

JOINT_NAMES = ("coxa", "femur", "tibia")  # matches JOINT_COXA/FEMUR/TIBIA (0-2)
CHANNEL_NAMES = ("total", "coxa", "femur", "tibia")  # matches ADC_CH_* (0-3)


@dataclass
class Sample:
    step: int
    raw_mv: float
    ground_truth_ma: float


@dataclass
class ChannelFit:
    channel: int
    samples: list = field(default_factory=list)
    scale: float = 0.0
    offset: float = 0.0
    r_squared: float = 0.0


class BoardPort:
    """A single line-oriented USB-serial command port (leg or calboard)."""

    def __init__(self, port: str):
        self.ser = serial.Serial(port, BAUD, timeout=1.0)
        time.sleep(2.0)  # arduino-pico USB CDC needs a moment after open
        self.ser.reset_input_buffer()

    def close(self):
        self.ser.close()

    def send(self, cmd: str) -> None:
        self.ser.write((cmd + "\n").encode("ascii"))

    def read_lines(self, n: int, timeout_s: float = 1.0) -> list:
        lines = []
        deadline = time.monotonic() + timeout_s
        while len(lines) < n and time.monotonic() < deadline:
            raw = self.ser.readline()
            if not raw:
                continue
            lines.append(raw.decode("ascii", errors="replace").strip())
        return lines

    def command(self, cmd: str, n_reply_lines: int = 1) -> list:
        self.send(cmd)
        return self.read_lines(n_reply_lines)

    def identify(self) -> str:
        self.send("PING")
        lines = self.read_lines(1)
        return lines[0] if lines else ""


def find_ports(leg_port: str | None, cal_port: str | None) -> tuple:
    """Return (leg_board_port, calboard_port), auto-detecting via PING if not given."""
    if leg_port and cal_port:
        return leg_port, cal_port

    candidates = [p.device for p in serial.tools.list_ports.comports()]
    found_leg, found_cal = leg_port, cal_port
    for dev in candidates:
        if found_leg and found_cal:
            break
        try:
            bp = BoardPort(dev)
        except (OSError, serial.SerialException):
            continue
        ident = bp.identify()
        bp.close()
        if not found_leg and "hexapod-leg" in ident:
            found_leg = dev
        elif not found_cal and "hexapod-calboard" in ident:
            found_cal = dev

    if not found_leg or not found_cal:
        raise SystemExit(
            f"could not auto-detect both boards (leg={found_leg}, cal={found_cal}); "
            f"pass --leg/--cal explicitly. Available ports: {candidates}"
        )
    return found_leg, found_cal


def parse_curraw(lines: list) -> dict:
    """Parses 4 lines of 'CHn <name> counts=<n> mv=<f>' -> {channel: mv}."""
    out = {}
    for line in lines:
        parts = line.split()
        if not parts or not parts[0].startswith("CH"):
            continue
        ch = int(parts[0][2:])
        mv = next((float(p.split("=", 1)[1]) for p in parts if p.startswith("mv=")), None)
        if mv is not None:
            out[ch] = mv
    return out


def parse_calboard_read(lines: list) -> dict:
    """Parses 3 lines of 'CHn <name> step=.. R_ohm=.. V=.. I_mA=.. mode=..' -> {channel: I_mA}."""
    out = {}
    for line in lines:
        parts = line.split()
        if not parts or not parts[0].startswith("CH"):
            continue
        ch = int(parts[0][2:])
        i_ma = next((float(p.split("=", 1)[1]) for p in parts if p.startswith("I_mA=")), None)
        if i_ma is not None:
            out[ch] = i_ma
    return out


def pulse_for_step(step: int) -> int:
    t = step / LADDER_STEPS
    return round(PWM_MIN_US + t * (PWM_MAX_US - PWM_MIN_US))


def linear_fit(xs: list, ys: list) -> tuple:
    """Least-squares y = scale*x + offset. Returns (scale, offset, r_squared)."""
    n = len(xs)
    mean_x = sum(xs) / n
    mean_y = sum(ys) / n
    ss_xx = sum((x - mean_x) ** 2 for x in xs)
    ss_xy = sum((x - mean_x) * (y - mean_y) for x, y in zip(xs, ys))
    if ss_xx == 0:
        raise ValueError("degenerate fit: all x values identical")
    scale = ss_xy / ss_xx
    offset = mean_y - scale * mean_x
    ss_tot = sum((y - mean_y) ** 2 for y in ys)
    ss_res = sum((y - (scale * x + offset)) ** 2 for x, y in zip(xs, ys))
    r_squared = 1.0 - (ss_res / ss_tot if ss_tot else 0.0)
    return scale, offset, r_squared


def sweep_joint(leg: BoardPort, cal: BoardPort, joint: int) -> Sample:
    """Steps one joint's channel through the ladder, yields raw/ground-truth pairs."""
    samples = []
    for step in range(1, LADDER_STEPS + 1):  # skip step 0: near-noise-floor, ambiguous
        pulse_us = pulse_for_step(step)
        leg.command(f"PWM {joint} {pulse_us}")
        time.sleep(SETTLE_S)

        raw = parse_curraw(leg.command("CURRAW?", n_reply_lines=4))
        gt = parse_calboard_read(cal.command("READ?", n_reply_lines=3))

        channel = joint + 1  # ADC_CH_TOTAL=0, COXA=1, FEMUR=2, TIBIA=3
        if channel not in raw or joint not in gt:
            print(f"  WARN step {step}: missing data (raw={raw.get(channel)}, gt={gt.get(joint)})")
            continue
        samples.append(Sample(step=step, raw_mv=raw[channel], ground_truth_ma=gt[joint]))
    return samples


def sweep_total(leg: BoardPort, cal: BoardPort) -> list:
    """Drives all 3 joints to the same step simultaneously; ground truth is
    the sum of the 3 calboard channels' currents."""
    samples = []
    for step in range(1, LADDER_STEPS + 1):
        pulse_us = pulse_for_step(step)
        for joint in range(3):
            leg.command(f"PWM {joint} {pulse_us}")
        time.sleep(SETTLE_S)

        raw = parse_curraw(leg.command("CURRAW?", n_reply_lines=4))
        gt = parse_calboard_read(cal.command("READ?", n_reply_lines=3))

        if 0 not in raw or len(gt) < 3:
            print(f"  WARN step {step}: missing data (raw={raw.get(0)}, gt={gt})")
            continue
        total_gt_ma = sum(gt.values())
        samples.append(Sample(step=step, raw_mv=raw[0], ground_truth_ma=total_gt_ma))
    return samples


def fit_channel(channel: int, samples: list) -> ChannelFit:
    xs = [s.raw_mv for s in samples]
    ys = [s.ground_truth_ma for s in samples]
    scale, offset, r_squared = linear_fit(xs, ys)
    return ChannelFit(channel=channel, samples=samples, scale=scale, offset=offset, r_squared=r_squared)


def push_calibration(leg: BoardPort, fit: ChannelFit) -> None:
    reply = leg.command(f"CURCAL {fit.channel} {fit.scale:.6f} {fit.offset:.3f}")
    print(f"  -> {reply[0] if reply else '(no reply)'}")


def write_csv(path, fits: list) -> None:
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["channel", "channel_name", "step", "raw_mv", "ground_truth_ma"])
        for fit in fits:
            for s in fit.samples:
                w.writerow([fit.channel, CHANNEL_NAMES[fit.channel], s.step, s.raw_mv, s.ground_truth_ma])


def main() -> int:
    global SETTLE_S
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--leg", help="LegBoard USB-serial port (auto-detected if omitted)")
    ap.add_argument("--cal", help="calboard USB-serial port (auto-detected if omitted)")
    ap.add_argument("--settle", type=float, default=SETTLE_S, help="settle time per step (s)")
    ap.add_argument("--out", default=None, help="CSV log output path (default: timestamped in cwd)")
    ap.add_argument("--no-push", action="store_true", help="fit and report only, don't write CURCAL to the leg")
    args = ap.parse_args()
    SETTLE_S = args.settle

    leg_port, cal_port = find_ports(args.leg, args.cal)
    print(f"LegBoard on {leg_port}, calboard on {cal_port}")

    leg = BoardPort(leg_port)
    cal = BoardPort(cal_port)

    fits = []
    try:
        for joint, name in enumerate(JOINT_NAMES):
            print(f"Sweeping {name} (joint {joint}, channel {joint + 1})...")
            samples = sweep_joint(leg, cal, joint)
            if len(samples) < 2:
                print(f"  SKIP: not enough samples ({len(samples)})")
                continue
            fits.append(fit_channel(joint + 1, samples))

        print("Sweeping total (all 3 joints together, channel 0)...")
        total_samples = sweep_total(leg, cal)
        if len(total_samples) >= 2:
            fits.append(fit_channel(0, total_samples))
        else:
            print(f"  SKIP: not enough samples ({len(total_samples)})")

        print()
        for fit in fits:
            print(
                f"CH{fit.channel} {CHANNEL_NAMES[fit.channel]}: "
                f"scale={fit.scale:.6f} mA/mV offset={fit.offset:.3f} mA "
                f"R^2={fit.r_squared:.4f} (n={len(fit.samples)})"
            )
            if fit.r_squared < 0.98:
                print(f"  WARN: low R^2 for CH{fit.channel} — check wiring/settle time before trusting this fit")

        if not args.no_push:
            print()
            print("Pushing calibration to LegBoard...")
            for fit in fits:
                push_calibration(leg, fit)

        out_path = args.out or f"current_calibration_{datetime.datetime.now():%Y%m%d_%H%M%S}.csv"
        write_csv(out_path, fits)
        print(f"Raw samples logged to {out_path}")
    finally:
        leg.close()
        cal.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())
