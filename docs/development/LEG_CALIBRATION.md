# LegBoard Configurator (Address + Current-Sense + Servo PWM/Sign Calibration)

## What this is

An interactive wizard for bringing up one LegBoard: assign its RS485
address, then guide a resistor-based calibration of its per-servo current
sense (INA4181 + shunt, one channel per servo), plus a separate flow to
recalibrate each servo's true physical center. Everything is driven
directly over the LegBoard's own USB serial console
(`firmware/leg/src/calib.cpp`) via
[`tools/leg_configurator.py`](../../tools/leg_configurator.py) — no separate
calibration board or RS485 link is needed, since the LegBoard already
exposes everything required (`ADDR` for identity, `CURRAW?`/`CURCAL` for
current calibration, `PWM`/`PWMNEUTRAL`/`INVERT` for servo PWM) locally.

(An earlier iteration of this tool used a second RS485-master board
("s-calib") on a spare LegBoard PCB, for a scenario where the target board
had no USB access of its own. That's not the common case — a LegBoard being
brought up is normally plugged straight into a PC — so it was dropped in
favor of talking to the target's own USB console directly.)

## Connect

Plug the target LegBoard into your PC via USB, then:

```bash
python tools/leg_configurator.py
```

The port is auto-detected (it looks for the RP2040's USB VID, so unrelated
ports like Bluetooth virtual COM ports are filtered out first) — pass
`--port COM10` explicitly if you ever need to override it, e.g. more than
one RP2040 board is plugged in at once.

(If you don't have `pyserial` in your default Python, run this with
PlatformIO's bundled interpreter instead — see the script's `--help`.)

## Modes (`--mode`)

The script covers three distinct jobs on one LegBoard; `--mode` picks which
to run:

| `--mode` | Runs |
|----------|------|
| `full` (default) | leg address → current-sense calibration → servo calibration, in sequence |
| `address` | just the leg address step, nothing else |
| `current` | current-sense calibration only (sections 3-5 below) |
| `servo` | servo calibration only: PWM center, direction check, range walk (below) |

`address` skips the board diagram (no wiring involved); `current` and
`servo` both show it first and confirm/set the address, since either one
needs you to know which physical connector is which. `--channels
coxa,femur,tibia` (default: all three) narrows which joints/channels
`current` and `servo` touch — same names, two different index namespaces
under the hood (current-sense channel vs. PWM joint).

## 1. Leg address

The wizard reads the board's current address first (`ADDR?`):

- If it's already set (1-6), that value is offered as the default — press
  Enter to keep it, or type a new one to change it.
- If the board is uncalibrated (address `0` — see `firmware/leg/README.md`
  "Uncalibrated-board marker"), you're required to enter one (1-6) before
  continuing.

Either way it's confirmed and persisted via `ADDR <n>` before moving on.

## 2. Board diagram

The wizard then prints an ASCII top-view of the LegBoard so you know which
physical connector is which before touching anything — J2/J3/J4 map to
coxa/femur/tibia respectively (confirmed against
`hardware/legboard/legboard_sch.py`, not just the silkscreen). It's a
hand-drawn approximation of the rendered board
(`hardware/legboard/board-front.png`), not pixel-derived from the image —
see `docs/plans/TODO.md` for auto-generating it from the real board render
as a possible follow-up.

## Current-sense calibration (`--mode current`, and part of `full`)

Guides a resistor-based calibration of the per-servo current sense (INA4181
+ shunt, one channel per servo plus `total`).

### 3. Zero-load offsets (all 4 channels at once)

The wizard asks you to disconnect all three servos, then Enter takes one
averaged `CURRAW?` reading (5 samples) that covers **total/coxa/femur/tibia
in a single step** — one poll already reports all 4 channels, so there's no
separate "now disconnect for total" pass. Whatever raw mV each channel
reports at zero load *is* that channel's offset error (true current is 0
there by definition).

This first prompt also takes **Esc instead of Enter** to skip current-sense
calibration entirely for this run, leaving whatever is already stored on the
board untouched — handy if you only came to check/adjust servo centers
(`--mode servo`) and don't need to redo current calibration.

Offset matters far more than scale for this hardware: the INA4181 gain +
shunt put scale theoretically at 1.0, and it lands around 0.97-0.99 in
practice — a small correction — while the offset can meaningfully skew
low-current readings if left uncorrected. So this single step alone is
already a reasonable calibration.

### 4. Choose how far to take it

After the offsets are measured, the wizard asks:

```
More precise calibration with resistors? [y/N], or enter a scale (0-2)
to use for every channel together with the offsets above:
```

- **Enter / `n` (default)** — write `scale=1.0` with the just-measured
  offset to all 4 channels and stop. Fastest path, good enough for most
  boards.
- **A number 0-2** — use that as the scale for all 4 channels (still paired
  with each channel's own measured offset). Handy if you already know this
  batch of boards runs close to e.g. 0.98 and want to skip the resistor
  dance entirely.
- **`y`** — go on to full per-channel resistor calibration (below), reusing
  the zero point already measured instead of re-measuring it per channel.

### 5. Resistor calibration (optional, `y` above)

For each of coxa/femur/tibia in turn, the wizard attaches a spread of known
resistor loads and fits a line through the (already-measured) zero point
plus all of them by least squares, calling out the exact connector (e.g.
"J2 (coxa)") at each step:

1. Attach each of four suggested THT resistor values in turn — **470 Ω,
   220 Ω, 100 Ω, 47 Ω** (common E12 values, low-to-high current). No
   confirmation keypress needed for the attach step itself: the wizard
   live-updates the raw reading on one line and auto-detects once it has
   moved away from the zero-point baseline and settled (on Windows, you can
   also just press Enter to force a manual reading, e.g. if the change is
   too small to trip the threshold). Once settled, it takes the recorded
   value as an average of 5 fresh individual readings — not just whichever
   single noisy poll happened to trip the detector. It then asks for the
   reference current (mA), defaulting to whatever you entered for that
   resistor value last time — press Enter to accept — or a theoretical `V/R`
   guess on the very first run. The wizard prints the minimum power rating
   needed per value (table below) — these are deliberately small, low-power
   loads; you do **not** need to source amps through a resistor.
2. The wizard fits `current_ma = scale × raw_mv + offset` by ordinary least
   squares over the zero point + all 4 span points and persists it with
   `CURCAL <ch> <scale> <offset>` — the same command `calib.cpp` already
   exposes for manual/bench calibration.
3. **`total` is calibrated too, for free.** `CURRAW?` reports all 4
   channels on every poll, so every coxa/femur/tibia span measurement above
   doubles as a `total` (channel 0) data point at no extra cost — no
   separate steps, no extra resistor attachments. The zero point from step 3
   is included too. After all selected branches are done, the wizard fits
   and persists `total` from whatever points got collected along the way.
   **This is only valid if the other two branches stay disconnected for the
   entire session**, not just their own step — the wizard prints a reminder
   before starting. If you only ran a subset of channels (`--channels`),
   `total`'s fit is based on just those branches' points.

**Redo a step, or just re-measure.** Made a mistake — wrong resistor,
mis-typed current, bumped a lead? At any reference-current prompt (or as a
bare keypress while a reading is live, on Windows):
- `b` discards the current step and redoes the *previous* one (as far back
  as the first resistor step — the zero point was already measured in step 3
  and isn't redone here).
- `r` re-measures the *current* step in place (same resistor, fresh
  reading) without losing your spot in the sequence.

| Load     | ≈ current @ 6V rail | Min. resistor power |
|----------|---------------------|----------------------|
| open     | 0 mA (zero point)   | —                    |
| 470 Ω    | ~13 mA              | 1/4 W                |
| 220 Ω    | ~27 mA              | 1/4 W (1/2 W recommended) |
| 100 Ω    | ~60 mA              | 1/2 W                |
| 47 Ω     | ~128 mA             | 1 W                  |

The "≈ current" column is only for picking a safe resistor — the wizard
always asks you to type in the *actual* measured/computed reference current
for each point, so real rail voltage and resistor tolerance don't matter for
accuracy.

**Extrapolation, not a full-range test.** These calibration points stay in
the tens-of-mA range on purpose — the INA4181 current-shunt amplifier is
linear by design, so a handful of low-power points is enough to fix the
line; there's no need (and no safe simple way) to physically source amps
through a small THT resistor. Pushing several amps through a resistor tied
to the servo's 6V rail dissipates tens of watts.

```bash
python tools/leg_configurator.py --mode current --port COM10                     # all three channels
python tools/leg_configurator.py --mode current --port COM10 --channels coxa     # just one
```

## Servo calibration (`--mode servo`, and part of `full`)

Recalibrates each servo's true physical center, checks that it moves the
right direction, and optionally walks its full safe range — all on an
assembled leg, driven purely over USB with the leg's own console (no RS485,
no mainboard needed). For each selected joint, in turn:

### 6. Center

A leg's true physical zero-degree pose isn't always reachable at exactly
1500 µs — a servo horn can't always be mounted perfectly centered on a given
leg's mechanical build, especially the femur/tibia links where a single
spline tooth of offset can be the difference between "close enough" and
visibly off. The wizard puts the joint into a live jog: the pulse width and
key legend redraw on one line as you go, and the leg moves in real time --

- **↑ / ↓** — fine adjust, ±5 µs (`JOG_STEP_FINE_US`)
- **← / →** — coarse adjust, ±25 µs (`JOG_STEP_COARSE_US`)
- **Enter** — accept the current value and persist it as the new center via
  `PWMNEUTRAL <joint> <us>`
- **Esc** — cancel, restoring whatever pulse width the joint had before you
  started jogging it (nothing persisted)

(Piped/non-interactive stdin, where arrow keys aren't available, falls back
to typing an exact pulse width plus Enter.)

Unlike a raw `PWM` override, accepting a new center **is** persisted (flash,
survives reboot) and takes effect immediately for normal operation too:
`servo_write_angle()` (used for every RS485-driven and interpolated move) is
anchored at the calibrated neutral, with independent linear spans out to the
(still compile-time) min/max pulse on either side — so recalibrating the
center shifts where a commanded angle of 0° physically lands without
distorting the endpoints.

### 7. Gentle direction check

Immediately after centering that joint, the wizard nudges it a small amount
(`--step` µs, default 100) to each side of the just-set center, one move at
a time — Enter to continue, `q` to quit — so you can watch which physical
direction each move produces before anything bigger happens. Current is
sampled at every step against a just-measured neutral baseline and flagged
if it jumps well above it (informational only — a heads-up to look closer,
not a hard cutoff, since the board may not be current-calibrated yet).

After both nudges it asks what direction you saw, and prints the direction a
*positive* commanded angle is supposed to produce per the mainboard IK's
leg-local convention — see
[`HARDWARE_AND_MECHANICS.md` "Joint Angle Sign Convention"](../architecture/HARDWARE_AND_MECHANICS.md#joint-angle-sign-convention)
— for femur and tibia (coxa's expected yaw direction depends on this leg's
mount rotation, which the wizard has no access to, so there's no universal
answer to print there). If what you saw doesn't match, it offers to flip and
persist that joint's sign (`INVERT <joint> <1|-1>`) right there and re-runs
the nudge so you can confirm the fix immediately, rather than discovering an
inverted joint later during an IK/gait test.

### 8. Extended range walk (optional)

After all selected joints' gentle checks, the wizard asks once whether to
also walk each joint toward its min/max pulse width in the same small
increments — still step-confirmed and current-monitored — to find the safe
usable range without jamming anything (leg geometry can make the real usable
range narrower than the servo's own 500-2500 µs).

All joints return to their (possibly just-recalibrated) center at the end,
or immediately if you quit early with `q`.

```bash
python tools/leg_configurator.py --mode servo --port COM10                  # all three joints
python tools/leg_configurator.py --mode servo --port COM10 --channels femur # just one
python tools/leg_configurator.py --mode servo --port COM10 --step 150       # bigger nudge
```

Query the current persisted centers and signs directly over the console at
any time:

```
PWMNEUTRAL?
J0 coxa neutral_us=1500
J1 femur neutral_us=1460
J2 tibia neutral_us=1500

INVERT?
J0 coxa invert=1
J1 femur invert=-1
J2 tibia invert=-1
```

Angle range (`angle_min_deg`/`angle_max_deg`) is not yet calibratable this
way — still a compile-time default (±90°), see `firmware/leg/README.md`
"Bring-Up Status/TODO".

## Zero-position build aid

`--zero` (independent of `--mode`) centers all three servos to each joint's
*calibrated* PWM neutral (`PWMNEUTRAL?`, previous section — 1500 µs /
`DEFAULT_PWM_NEUTRAL_US` until recalibrated) via the raw `PWM <joint> <us>`
override, without running any calibration, so the leg can be physically
assembled against a fixed mechanical reference:

```bash
python tools/leg_configurator.py --port COM10 --zero
```

This is a raw override, not persisted — it releases as soon as the board
gets a real target (e.g. once it's wired to the mainboard) or power-cycles.
(The PWM neutral it centers to, unlike this override itself, *is*
persisted.)

## Hardware current range

The LegBoard's INA4181A3 (100 V/V, fixed) + 10 mΩ Kelvin shunts
(`hardware/legboard/legboard_sch.py`, all four channels identical today) cap
out at:

```
I_max = Vref / (Gain × Rshunt) = 3.3 V / (100 × 0.01 Ω) ≈ 3.3 A per channel
```

That's a hard ADC-saturation ceiling — no calibration, software or
otherwise, can measure above it on the current hardware. It's adequate for
a single servo branch (coxa/femur/tibia), but the `total` channel sums up to
three branches and can see meaningfully more under worst-case simultaneous
stall. The wizard's `total` fit (previous section) is still just an
extrapolation from tens-of-mA points, same as the branches — it doesn't
change this ceiling. A shunt change for the total channel only (R4:
10 mΩ → ~3 mΩ, raising its ceiling to ~11 A) is planned but not yet done —
see `docs/plans/TODO.md`. Until then, treat ~3.3 A as the real ceiling on
every channel, including `total`.

## Related docs

- [`../interfaces/RS485_PROTOCOL.md`](../interfaces/RS485_PROTOCOL.md) — wire protocol (current-sense calibration is deliberately not part of it — see "Stored Parameters")
- [`../../firmware/leg/README.md`](../../firmware/leg/README.md) — the LegBoard's own firmware and USB console
- [`../../tools/serial_console.py`](../../tools/serial_console.py) — generic line-buffered serial console (manual `calib.cpp` commands, local echo, sends on Enter) if you'd rather drive it by hand
