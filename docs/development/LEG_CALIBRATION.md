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
python tools/leg_configurator.py
```

The port is auto-detected (it looks for the RP2040's USB VID, so unrelated
ports like Bluetooth virtual COM ports are filtered out first) — pass
`--port COM10` explicitly if you ever need to override it, e.g. more than
one RP2040 board is plugged in at once.

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

## 3. Zero-load offsets (all 4 channels at once)

The wizard asks you to disconnect all three servos, then takes one averaged
`CURRAW?` reading (5 samples) that covers **total/coxa/femur/tibia in a
single step** — one poll already reports all 4 channels, so there's no
separate "now disconnect for total" pass. Whatever raw mV each channel
reports at zero load *is* that channel's offset error (true current is 0
there by definition).

Offset matters far more than scale for this hardware: the INA4181 gain +
shunt put scale theoretically at 1.0, and it lands around 0.97-0.99 in
practice — a small correction — while the offset can meaningfully skew
low-current readings if left uncorrected. So this single step alone is
already a reasonable calibration.

## 4. Choose how far to take it

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

## 5. Resistor calibration (optional, `y` above)

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
