#!/usr/bin/env python3
"""Cartesian motion tests for one leg, driven through the mainboard's RPC
`ik <leg> <x> <y> <z>` command (leg-local frame: X out, Y forward, Z up).

Two modes:

--mode axes (default): confirms IK behaves correctly along each axis
independently -- for X, Y, then Z, in both the positive and negative
direction (6 tests total), smoothly moves the foot tip 2cm from the neutral
pose and back, holding the other two axes fixed at their neutral value.
Note: a pure Y sweep at this leg's working radius is expected to move mostly
coxa (yaw), with femur/tibia changing only a fraction of a degree -- that's
correct kinematics (Y is nearly tangential to the hip's rotation arc at this
radius), not a bug. X and Z sweeps engage femur+tibia together and show that
coupling much more visibly.

--mode circle: sweeps a full circle of the given radius in the chosen plane
(--plane xy|xz|yz, default xy), centered on --center (default: the neutral
point). In the xy plane, every point other than the +-X extremes requires
coxa, femur, *and* tibia together, so it's a clearer multi-joint coupling
demonstration than a single Y sweep.

--mode figure8: sweeps a figure-8 (lemniscate of Gerono) in the chosen plane,
centered on --center, using the same go-to-center/confirm/sweep/hold protocol
as circle mode. --radius is half the figure's height: the two lobes span
+-radius along the plane's first-named axis (e.g. X for "xy"/"xz") and
+-radius/2 along the second (e.g. Y for "xy", Z for "xz").

All three modes move at constant speed along a fixed-size step (--resolution,
meters/step) rather than a step *rate* -- path length / resolution gives the
step count directly, so the path is smooth regardless of speed, and
--speed (m/s, i.e. m of path per second) sets how fast it's walked. Sent as
a rapid sequence of `ik` commands over one persistent RPC connection, not a
single jump. The actual update interval is resolution/speed -- if motion
looks jittery, shrink --resolution (not --speed, which would just move
faster at the same jitter). Measured ceiling for this leg: the RPC round
trip itself supports ~60Hz, but hex_rs485_master's own round-robin poll only
updates this leg's *commanded* target every ~20ms (~50Hz, less if other legs
are connected and timing out slower/faster) -- pushing resolution/speed much
past that just adds RPC traffic with no visible smoothing.

Neutral is computed live from the leg's actual configured geometry
(`get leg_geom leg<N>_len_*`), not hardcoded, so it stays correct if lengths
change: (x, y, z) = (len_coxa + len_femur, 0, -len_tibia) -- see
docs/architecture/HARDWARE_AND_MECHANICS.md "Joint Angle Sign Convention"
for why that's the neutral (all-zero-angle) pose.

Usage:
    python3 tools/ik_axis_test.py
    python3 tools/ik_axis_test.py --leg 1 --step 0.02 --speed 0.02
    python3 tools/ik_axis_test.py --mode circle --radius 0.04 --speed 0.03
    python3 tools/ik_axis_test.py --mode circle --center 0.12 0 -0.063 --plane xz --radius 0.03
    python3 tools/ik_axis_test.py --mode figure8 --center 0.17 0 -0.063 --plane xy --radius 0.03
"""
import argparse
import math
import socket
import time

AXES = ["x", "y", "z"]


class Rpc:
    def __init__(self, host, port):
        self.sock = socket.create_connection((host, port), timeout=5)
        self.sock.settimeout(0.5)

    def send(self, cmd):
        self.sock.sendall((cmd + "\n").encode())

    def drain(self):
        """Truly non-blocking: grabs whatever response bytes have already
        arrived and returns immediately either way. Used between animation
        steps so the socket's receive buffer doesn't back up, without
        stalling the step rate on a recv() timeout (the previous version of
        this used the socket's blocking 0.5s timeout here, which meant every
        single step stalled up to ~0.5-1s waiting for a timeout that would
        never come, since the mainboard responds to every `ik`. That's what
        produced the ~2Hz, non-circular-looking motion.)"""
        self.sock.settimeout(0.0)
        try:
            while self.sock.recv(4096):
                pass
        except (BlockingIOError, socket.timeout):
            pass
        finally:
            self.sock.settimeout(0.5)

    def command(self, cmd, timeout=2.0):
        self.send(cmd)
        self.sock.settimeout(timeout)
        buf = b""
        try:
            buf = self.sock.recv(4096)
        except socket.timeout:
            pass
        self.sock.settimeout(0.5)
        return buf.decode(errors="replace").strip()

    def close(self):
        self.sock.close()


def detect_first_available_leg(rpc):
    """Picks the first leg (1-6) that has ever responded on the RS485 bus,
    via `pos`, rather than assuming leg 1 is the one wired up -- during
    bring-up/calibration it's common for only one leg to be connected, and
    not necessarily leg 1."""
    for leg in range(1, 7):
        if "has never responded" not in rpc.command(f"pos {leg}"):
            return leg
    raise SystemExit("no leg responded on the RS485 bus (checked legs 1-6)")


def get_leg_geometry(rpc, leg_index):
    coxa = float(rpc.command(f"get leg_geom leg{leg_index}_len_coxa").split("=")[1])
    femur = float(rpc.command(f"get leg_geom leg{leg_index}_len_femur").split("=")[1])
    tibia = float(rpc.command(f"get leg_geom leg{leg_index}_len_tibia").split("=")[1])
    return coxa, femur, tibia


def ik(rpc, leg, x, y, z):
    rpc.send(f"ik {leg} {x:.4f} {y:.4f} {z:.4f}")
    rpc.drain()


def move_linear(rpc, leg, start, end, resolution, speed):
    """Straight-line interpolation from start to end, stepping every
    `resolution` meters of path (not a fixed step count), at `speed` m/s --
    constant speed along the line."""
    length = math.dist(start, end)
    steps = max(1, math.ceil(length / resolution))
    dt = resolution / speed
    for i in range(1, steps + 1):
        t = i / steps
        x = start[0] + (end[0] - start[0]) * t
        y = start[1] + (end[1] - start[1]) * t
        z = start[2] + (end[2] - start[2]) * t
        ik(rpc, leg, x, y, z)
        time.sleep(dt)


def circle_point(center, radius, plane, theta):
    """Point at angle `theta` on a circle of `radius` around `center`, in the
    given plane ('xy'|'xz'|'yz') -- the two named axes vary, the third stays
    fixed at its center value."""
    cx, cy, cz = center
    c, s = radius * math.cos(theta), radius * math.sin(theta)
    if plane == "xy":
        return (cx + c, cy + s, cz)
    elif plane == "xz":
        return (cx + c, cy, cz + s)
    elif plane == "yz":
        return (cx, cy + c, cz + s)
    raise ValueError(f"unknown plane: {plane!r}")


def sweep_circle(rpc, leg, center, radius, plane, resolution, speed, loops=1):
    """`loops` full circles (theta: 0 -> loops*2*pi) in the given plane around
    `center`, stepping every `resolution` meters of arc length (not a fixed
    step count), at `speed` m/s -- constant speed around the circumference.
    Ends back at the start point regardless of `loops` (theta wraps exactly)."""
    circumference = 2 * math.pi * radius
    steps_per_loop = max(1, math.ceil(circumference / resolution))
    dt = resolution / speed
    total_steps = steps_per_loop * loops
    for i in range(total_steps + 1):
        theta = 2 * math.pi * i / steps_per_loop
        x, y, z = circle_point(center, radius, plane, theta)
        ik(rpc, leg, x, y, z)
        time.sleep(dt)


def figure8_point(center, radius, plane, t):
    """Point at parameter `t` on a figure-8 (lemniscate of Gerono) around
    `center`, in the given plane. `radius` is half the figure's height: the
    lobes span +-radius along the plane's first-named axis and +-radius/2
    along the second. t=0 and t=2*pi both land at `center` (the crossing
    point)."""
    cx, cy, cz = center
    a = radius * math.sin(t)                       # -radius .. radius
    b = radius * math.sin(t) * math.cos(t)          # -radius/2 .. radius/2
    if plane == "xy":
        return (cx + a, cy + b, cz)
    elif plane == "xz":
        return (cx + a, cy, cz + b)
    elif plane == "yz":
        return (cx, cy + a, cz + b)
    raise ValueError(f"unknown plane: {plane!r}")


def figure8_path_length(center, radius, plane, samples=2000):
    """Numerically estimated arc length of one full figure-8 loop -- unlike
    a circle's circumference, the lemniscate's arc length has no closed
    form, so this sums consecutive-point distances over a fine sample."""
    total = 0.0
    prev = figure8_point(center, radius, plane, 0.0)
    for i in range(1, samples + 1):
        t = 2 * math.pi * i / samples
        cur = figure8_point(center, radius, plane, t)
        total += math.dist(prev, cur)
        prev = cur
    return total


def sweep_figure8(rpc, leg, center, radius, plane, resolution, speed, loops=1):
    """`loops` full figure-8 loops (t: 0 -> loops*2*pi), stepping every
    `resolution` meters of arc length (not a fixed step count), at `speed`
    m/s -- constant speed along the path. Ends back at the crossing point
    (=center) regardless of `loops`."""
    length = figure8_path_length(center, radius, plane)
    steps_per_loop = max(1, math.ceil(length / resolution))
    dt = resolution / speed
    total_steps = steps_per_loop * loops
    for i in range(total_steps + 1):
        t = 2 * math.pi * i / steps_per_loop
        x, y, z = figure8_point(center, radius, plane, t)
        ik(rpc, leg, x, y, z)
        time.sleep(dt)


def run_axis_test(rpc, leg, neutral, axis_index, sign, step, resolution, speed):
    label = f"{AXES[axis_index]}{'+' if sign > 0 else '-'}"
    target = list(neutral)
    target[axis_index] += sign * step
    print(f"\n=== axis {label} ({sign * step * 100:+.0f} cm) ===")
    move_linear(rpc, leg, neutral, target, resolution, speed)
    time.sleep(0.3)
    direction = input("  which way did the tip move? ").strip() or "(no answer)"
    move_linear(rpc, leg, target, neutral, resolution, speed)
    return direction


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default="192.168.4.1")
    ap.add_argument("--port", type=int, default=5555)
    ap.add_argument("--leg", type=int, default=None,
                     help="leg number, 1-6 (default: first leg that responds on the bus)")
    ap.add_argument("--mode", choices=["axes", "circle", "figure8"], default="axes")
    ap.add_argument("--step", type=float, default=0.02, help="[axes mode] sweep distance in meters (default 2cm)")
    ap.add_argument("--radius", type=float, default=0.04,
                     help="[circle/figure8 mode] circle radius, or half the figure-8's height, in meters (default 4cm)")
    ap.add_argument("--center", type=float, nargs=3, metavar=("X", "Y", "Z"), default=None,
                     help="[circle/figure8 mode] shape center (default: this leg's neutral point)")
    ap.add_argument("--plane", choices=["xy", "xz", "yz"], default="xy",
                     help="[circle/figure8 mode] which plane the shape is drawn in (default xy)")
    ap.add_argument("--loops", type=int, default=1, help="[circle/figure8 mode] number of full revolutions (default 1)")
    ap.add_argument("--release", action="store_true",
                     help="[circle/figure8 mode] release to gait/IK control when done (default: hold position)")
    ap.add_argument("--resolution", type=float, default=0.0005,
                     help="path step size in meters (default 0.5mm) -- smaller = smoother, more RPC traffic. "
                          "Step interval is resolution/speed, so at the default 2cm/s this is ~25ms (~40Hz), "
                          "close to the measured ~20ms RS485 update-rate ceiling for this leg -- going much "
                          "smaller just wastes RPC traffic without visibly smoother motion.")
    ap.add_argument("--speed", type=float, default=0.02, help="path speed in m/s (default 2cm/s)")
    args = ap.parse_args()

    print(f"Connecting to mainboard RPC at {args.host}:{args.port} ...")
    rpc = Rpc(args.host, args.port)
    print(rpc.command("version"))

    if args.leg is None:
        args.leg = detect_first_available_leg(rpc)
        print(f"--leg not given; auto-detected leg {args.leg} as first available")
    leg_index = args.leg - 1

    coxa, femur, tibia = get_leg_geometry(rpc, leg_index)
    neutral = (coxa + femur, 0.0, -tibia)
    print(f"leg {args.leg} geometry: coxa={coxa:.3f} femur={femur:.3f} tibia={tibia:.3f}")
    print(f"neutral target: x={neutral[0]:.4f} y={neutral[1]:.4f} z={neutral[2]:.4f}")

    try:
        if args.mode == "axes":
            input("\nPress Enter to move to neutral and confirm it looks right before testing...")
            ik(rpc, args.leg, *neutral)  # single ik call to settle at neutral
            input("Neutral OK? Press Enter to start the 6-direction sweep test...")
            summary = {}
            for axis_index in range(3):
                for sign in (1, -1):
                    label = f"{AXES[axis_index]}{'+' if sign > 0 else '-'}"
                    summary[label] = run_axis_test(
                        rpc, args.leg, neutral, axis_index, sign, args.step, args.resolution, args.speed)
            print("\n=== Summary ===")
            for label, direction in summary.items():
                print(f"  {label}: {direction}")
        else:
            center = tuple(args.center) if args.center is not None else neutral
            shape = "circle" if args.mode == "circle" else "figure-8"
            print(f"\n=== {shape}: center=(x={center[0]:.4f}, y={center[1]:.4f}, z={center[2]:.4f}) "
                  f"radius={args.radius:.3f} plane={args.plane} loops={args.loops} ===")
            # Direct jump to the center -- not neutral, and not the shape's
            # edge/start point. The first step of the sweep itself is what
            # moves out from there (circle: to the edge; figure8: the
            # crossing point *is* the center, so this is already the start).
            ik(rpc, args.leg, *center)
            input(f"At the {shape}'s center. Press Enter to sweep...")
            if args.mode == "circle":
                sweep_circle(rpc, args.leg, center, args.radius, args.plane, args.resolution, args.speed, args.loops)
            else:
                sweep_figure8(rpc, args.leg, center, args.radius, args.plane, args.resolution, args.speed, args.loops)
            print(f"{shape.capitalize()} complete, back at the start point.")
    finally:
        # axes mode always releases (existing behavior). circle/figure8 only
        # release on request -- default is to hold position, whether the
        # script finished normally or exited early (Ctrl-C, error): gait/IK's
        # own idle output looks like neutral, which is exactly what these
        # modes are trying to avoid snapping back to.
        if args.mode == "axes" or args.release:
            rpc.command(f"joint {args.leg} release")
        else:
            print("Holding position (pass --release to hand back to gait/IK).")
        rpc.close()


if __name__ == "__main__":
    main()
