# LegBoard current-sensor calibration

Calibrates a LegBoard's INA4181 current sensing (see
`firmware/leg/src/current.cpp`) against a known load, instead of a
multimeter and manual math each time. Meant to be run once per LegBoard (and
again after any hardware rev).

## Hardware setup

1. Plug the calibration adapter board (`hardware/calboard/`,
   `firmware/calboard/`) into the LegBoard's J2/J3/J4 servo connectors in
   place of real servos.
2. Power the LegBoard normally (RS485 not required — this script talks to it
   directly over its USB calibration serial port).
3. Plug both the LegBoard and the calboard into this PC over USB.

## Usage

```bash
pip install -r requirements.txt
python calibrate_current.py                    # auto-detects both boards' ports
python calibrate_current.py --leg COM5 --cal COM7   # or specify explicitly
python calibrate_current.py --no-push           # fit + report only, don't write to the leg's flash
```

The script:
1. Steps each joint's channel (coxa/femur/tibia) through the calboard's
   15-step resistor ladder via the LegBoard's `PWM <joint> <us>` command,
   pairing the LegBoard's raw ADC reading (`CURRAW?`) against the calboard's
   computed ground-truth current (`READ?`) at each step.
2. Repeats with all three joints driven together for the `total` channel,
   using the sum of the three calboard channels as ground truth.
3. Least-squares fits `mA = scale * raw_mV + offset` per channel and reports
   R² (warns below 0.98 — check wiring/settle time before trusting a low-R²
   fit).
4. Pushes each channel's `scale`/`offset` to the LegBoard via `CURCAL`
   (persisted to flash — see `firmware/leg/src/persist.cpp`), unless
   `--no-push` is given.
5. Writes a timestamped CSV of every raw sample for traceability.

See `firmware/leg/src/calib.cpp` and `firmware/calboard/src/usbserial.cpp`
for the exact command protocols this script drives.
