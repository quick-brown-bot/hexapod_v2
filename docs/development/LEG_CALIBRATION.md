# LegBoard Configurator (Address + Current-Sense Calibration)

## What this is

An interactive wizard for bringing up one LegBoard: assign its RS485
address, then guide a resistor-based calibration of its per-servo current
sense (INA4181 + shunt, one channel per servo). Everything is driven
directly over the LegBoard's own USB serial console
(`firmware/leg/src/calib.cpp`) via
[`tools/leg_configurator.py`](../../tools/leg_configurator.py) — no separate
calibration board or RS485 link is needed, since the LegBoard already
exposes everything required (`ADDR` for identity, `CURRAW?`/`CURCAL` for
current calibration, `PWM` for a raw servo override) locally.

(An earlier iteration of this tool used a second RS485-master board
("s-calib") on a spare LegBoard PCB, for a scenario where the target board
had no USB access of its own. That's not the common case — a LegBoard being
brought up is normally plugged straight into a PC — so it was dropped in
favor of talking to the target's own USB console directly.)

## Connect

Plug the target LegBoard into your PC via USB, then:

```bash
python tools/leg_configurator.py --port COM10   # or whatever port it enumerates as
```

(If you don't have `pyserial` in your default Python, run this with
PlatformIO's bundled interpreter instead — see the script's `--help`.)

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

## 3. Current-sense calibration (coxa / femur / tibia)

For each channel in turn, the wizard walks through a **zero point first,
then a spread of known resistor loads**, and fits a line through all of them
by least squares (not just two points), calling out the exact connector
(e.g. "J2 (coxa)") at each step:

1. **Zero point.** Disconnect the servo from this channel (open circuit — no
   load) and confirm. Reads the raw ADC millivolt value via `CURRAW?`. Done
   first, before any resistors, so it both anchors the fit at the one
   reference current we know exactly (0 mA) and immediately catches a
   disconnected/miswired channel before you spend time on resistor points.
2. **Span points.** Attach each of four suggested THT resistor values in
   turn — **470 Ω, 220 Ω, 100 Ω, 47 Ω** (common E12 values, low-to-high
   current) — confirming and entering the measured/computed reference
   current (mA) for each. The wizard prints the minimum power rating needed
   per value (table below) — these are deliberately small, low-power loads;
   you do **not** need to source amps through a resistor.
3. The wizard fits `current_ma = scale × raw_mv + offset` by ordinary least
   squares over all 5 points (1 zero + 4 span) and persists it with
   `CURCAL <ch> <scale> <offset>` — the same command `calib.cpp` already
   exposes for manual/bench calibration.

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
python tools/leg_configurator.py --port COM10                     # all three channels
python tools/leg_configurator.py --port COM10 --channels coxa     # just one
```

## Zero-position build aid

`--zero` runs the address step and board diagram, then centers all three
servos to the firmware's neutral pulse width (1500 µs,
`DEFAULT_PWM_NEUTRAL_US`) via the existing `PWM <joint> <us>` raw override
command instead of running current calibration, so the leg can be physically
assembled against a fixed mechanical reference:

```bash
python tools/leg_configurator.py --port COM10 --zero
```

This is a raw override, not persisted — it releases as soon as the board
gets a real target (e.g. once it's wired to the mainboard) or power-cycles.

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
stall — which is also why `total` isn't covered by this wizard: it sees the
sum of all three branches at once, so it can't be isolated with a single
test resistor the way each branch can. A shunt change for the total channel
only (R4: 10 mΩ → ~3 mΩ, raising its ceiling to ~11 A) is planned but not
yet done — see `docs/plans/TODO.md`. Until then, treat ~3.3 A as the real
ceiling on every channel, including `total`.

## Related docs

- [`../interfaces/RS485_PROTOCOL.md`](../interfaces/RS485_PROTOCOL.md) — wire protocol (current-sense calibration is deliberately not part of it — see "Stored Parameters")
- [`../../firmware/leg/README.md`](../../firmware/leg/README.md) — the LegBoard's own firmware and USB console
- [`../../tools/serial_console.py`](../../tools/serial_console.py) — generic line-buffered serial console (manual `calib.cpp` commands, local echo, sends on Enter) if you'd rather drive it by hand
