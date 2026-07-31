# RS485 Communication Protocol — V2

## Purpose

This document specifies the RS485 communication protocol between the ESP32
MainBoard and the six RP2040 LegBoards in the V2 system.

It covers the transaction model, frame format, stored parameters, and timing
budget.

For the firmware architecture that drives this protocol see
[`../architecture/SYSTEM_ARCHITECTURE.md`](../architecture/SYSTEM_ARCHITECTURE.md).
For hardware wiring and electrical details see
[`../architecture/HARDWARE_AND_MECHANICS.md`](../architecture/HARDWARE_AND_MECHANICS.md).

---

## Design Principles

- The ESP32 is the sole bus master. LegBoards never transmit unsolicited.
- Every transaction is request-response. There are no fire-and-forget frames.
- There is one frame type. Every pull from the ESP32 carries desired joint
  angles and optionally carries pending configuration. The LegBoard always
  responds with a telemetry frame.
- A successful response implicitly acknowledges any configuration included in
  that pull. A timeout leaves configuration pending — it is included again in
  the next pull to that leg automatically.
- Encoding is **text (ASCII)**. Frames are human-readable for ease of
  bring-up and debugging with a serial monitor.
- The bus is half-duplex, direction controlled by `RS485_DE`.
- All six legs share a single multi-drop bus. Transactions are sequential.
- The main locomotion loop never blocks on bus I/O. All transactions are
  performed by the `hex_rs485_master` FreeRTOS task.

---

## Physical Layer

- **Baud rate**: 1 000 000 bps (1 Mbps)
- **UART framing**: 8N1 (10 bits per byte → 10 µs per byte at 1 Mbps)
- **Bus**: half-duplex RS485, multi-drop, 6 legs on one pair
- **Termination**: 120 Ω at each physical end of the cable run. Populate on
  the two legs at the ends of the run; DNP (do not populate) on mid-bus legs.

---

## Frame Format

All frames are ASCII text terminated with `\n`. A `*XX` suffix carries a
CRC8 checksum as two uppercase hex digits, covering all bytes from (and
including) the leading `>` or `<` up to (not including) the `*`.

**CRC8 algorithm**: CRC-8/SMBus — polynomial `0x07`, initial value `0x00`,
no input/output reflection, no final XOR. Both the ESP32 master and the RP2040
leg firmware must use this identical algorithm. Reference implementation:
`firmware/mainboard/components/hex_rs485_master/rs485_master.c` (`crc8()`).

Config parameter IDs in `Pii=` entries are written as two **hex** digits
(e.g. `P01`, `P0A`), matching the parameter IDs in the Stored Parameters table.
Parameter *values* are decimal.

### Pull Frame (ESP32 → LegBoard)

```
>[addr],[flags],[coxa],[femur],[tibia][,Pii=vvv...]*[crc]\n
```

| Field | Format | Description |
|-------|--------|-------------|
| `>` | literal | Frame start |
| `[addr]` | `1`–`6` | Leg address |
| `[flags]` | 2-digit hex | Option flags (see below) |
| `[coxa]` | decimal, 1 d.p. | Desired coxa angle in degrees, e.g. `-45.5` |
| `[femur]` | decimal, 1 d.p. | Desired femur angle in degrees |
| `[tibia]` | decimal, 1 d.p. | Desired tibia angle in degrees |
| `[,Pii=vvv]` | optional, repeating | Config entry: `P` + 2-digit param ID + `=` + value |
| `*[crc]` | `*XX` | CRC8 of frame content before `*` |
| `\n` | literal | Frame end |

#### Pull Flags

| Bit | Mask | Name | Description |
|-----|------|------|-------------|
| 0 | `01` | `JOINT_POS` | Request current joint positions in the telemetry response |
| 1–7 | — | — | Reserved, set to `00` |

#### Examples

Normal pull, no options:
```
>1,00,-45.5,-30.2,60.0*F5\n
```

Pull requesting joint positions in response:
```
>1,01,-45.5,-30.2,60.0*DF\n
```

Pull with a pending config update (move duration = 10 ms):
```
>1,00,-45.5,-30.2,60.0,P01=10*24\n
```

Pull with joint positions requested and two config entries:
```
>1,01,-45.5,-30.2,60.0,P01=10,P02=500*3A\n
```

> The `*XX` values above are real CRC-8/SMBus checksums over the frame content
> (from `>`/`<` up to but not including `*`). They double as test vectors for
> the master and leg CRC implementations.

---

### Telemetry Response (LegBoard → ESP32)

```
<[addr],[status],[itot],[icoxa],[ifemur],[itibia][,[coxa],[femur],[tibia]]*[crc]\n
```

| Field | Format | Description |
|-------|--------|-------------|
| `<` | literal | Frame start |
| `[addr]` | `1`–`6` | Echo of leg address |
| `[status]` | 2-digit hex | Status flags (see below) |
| `[itot]` | integer (mA) | Total leg current |
| `[icoxa]` | integer (mA) | Coxa servo current |
| `[ifemur]` | integer (mA) | Femur servo current |
| `[itibia]` | integer (mA) | Tibia servo current |
| `[,[coxa],[femur],[tibia]]` | decimal, 1 d.p. | Current joint positions in degrees. Present only when `JOINT_POS` was set in the pull flags. |
| `*[crc]` | `*XX` | CRC8 |
| `\n` | literal | Frame end |

#### Response Status Flags

| Bit | Mask | Name | Description |
|-----|------|------|-------------|
| 0 | `01` | `CONFIG_APPLIED` | Config entries from this pull were applied |
| 1 | `02` | `WATCHDOG_ACTIVE` | Holding last position — bus was silent beyond timeout |
| 2 | `04` | `LIMIT_CLAMP` | At least one joint was clamped to its hard limit this cycle |
| 3–7 | — | — | Reserved |

#### Examples

Normal response, no flags:
```
<1,00,1250,450,380,420*71\n
```

Response acknowledging a config update:
```
<1,01,1250,450,380,420*5B\n
```

Response with joint positions (when `JOINT_POS` was set in pull):
```
<1,00,1250,450,380,420,-44.8,-29.5,59.2*77\n
```

---

## Transaction Model

`hex_rs485_master` executes the following for each leg in sequence:

```
For leg N = 1 to 6:
  1. Assemble pull frame:
       — always: address, flags, desired joint angles
       — if config pending for leg N: append config entries, set no extra flag
         (the presence of Pii= entries is self-indicating)
       — if joint positions were requested this cycle: set JOINT_POS flag
  2. Assert DE high (ESP32 transmit)
  3. Send pull frame
  4. Release DE low (ESP32 receive)
  5. Wait for '\n'-terminated response within timeout
  6a. Valid response received and CRC passes:
       — write current readings and status to telemetry buffer for leg N
       — if pull contained config entries: mark them applied for leg N
  6b. Timeout or CRC failure:
       — mark telemetry buffer entry for leg N as stale
       — config entries remain pending: included again next pull
Repeat from leg 1.
```

The RP2040 on leg N:

```
  1. Listens continuously; ignores frames addressed to other legs
  2. Receives until '\n', validates CRC
  3. On valid frame:
       — updates interpolation target with received joint angles
       — applies any Pii= config entries found in the frame
       — sets CONFIG_APPLIED in response status if any config was applied
       — sets JOINT_POS values in response if JOINT_POS flag was set in pull
  4. Asserts DE high
  5. Sends telemetry response
  6. Releases DE low
  7. On CRC failure: does not respond (produces timeout on master)
```

---

## Stored Parameters

The RP2040 stores parameters locally. The ESP32 queues updates per leg and
delivers them piggybacked on the next pull. A successful response clears the
pending entry. A timeout leaves it pending for the next pull.

| ID | Parameter | Type | Default | Description |
|----|-----------|------|---------|-------------|
| `01` | `MOVE_DURATION` | uint16 (ms) | `10` | Interpolation window. Not sent per pull; updated only on change. |
| `02` | `WATCHDOG_TIMEOUT` | uint16 (ms) | `500` | Hold-last-position timeout on bus silence |
| `03` | `INTERP_MODE` | uint8 | `00` | `00` = LINEAR, `01` = CUBIC. The leg boots LINEAR for bring-up; CUBIC is opt-in via this parameter. |
| `04` | `COXA_LIMIT_MIN` | int16 (0.1°) | TBD | Coxa minimum angle hard limit |
| `05` | `COXA_LIMIT_MAX` | int16 (0.1°) | TBD | Coxa maximum angle hard limit |
| `06` | `FEMUR_LIMIT_MIN` | int16 (0.1°) | TBD | Femur minimum angle hard limit |
| `07` | `FEMUR_LIMIT_MAX` | int16 (0.1°) | TBD | Femur maximum angle hard limit |
| `08` | `TIBIA_LIMIT_MIN` | int16 (0.1°) | TBD | Tibia minimum angle hard limit |
| `09` | `TIBIA_LIMIT_MAX` | int16 (0.1°) | TBD | Tibia maximum angle hard limit |

On RP2040 reboot all parameters reset to firmware defaults. The ESP32 detects
leg recovery (a response after a run of timeouts) and re-queues all non-default
parameters for that leg.

> **TBD** — Servo limit defaults. Derive from measured physical range during
> calibration. Until calibrated: wide defaults that prevent damage but allow the
> full intended motion range.

> **TBD** — Whether the RP2040 persists parameters to flash across reboots.
> Simplest first: firmware defaults only, ESP32 re-applies on recovery.

---

## Timing Budget

**Target**: all 6 legs polled within 10 ms.

At 1 Mbps / 8N1: **10 µs per byte**.

| Step | Bytes | Time |
|------|-------|------|
| Pull frame (typical) | 26 | 260 µs |
| DE turnaround (× 2) | — | 40 µs |
| RP2040 receive + respond | — | 150 µs |
| Telemetry response (typical) | 26 | 260 µs |
| **Per-leg total** | | **~710 µs** |
| **6 legs** | | **~4.3 ms** |

Headroom: ~5.7 ms. This accommodates occasional larger frames (config entries,
joint positions in response) and RP2040 processing jitter without exceeding the
10 ms budget.

Worst-case frame sizes:
- Pull with two config entries: ~45 bytes → 450 µs
- Response with joint positions: ~40 bytes → 400 µs
- Worst-case per-leg: ~1040 µs → 6 legs: ~6.3 ms — still within budget.

> **Note** — Text encoding is chosen for bring-up and debuggability. If the
> timing budget becomes constrained in future (e.g. more telemetry fields,
> faster gait requirements), switching to a binary frame format would reduce
> per-leg transaction time by roughly 3×.

### Measured Timing (Hardware Bring-Up, single leg)

First real hardware measurement, MainBoard ESP32 talking to one physically
wired LegBoard (address 1) over the actual RS485 link — not simulated. Bench
tool: `firmware/mainboard_rs485_test` (standalone ESP-IDF app, not part of
the production `hex_rs485_master` build). Round-trip time (RTT) measured from
the start of the pull-frame transmission to the fully received `\n`-terminated
response.

| Scenario | n | Result |
|---|---|---|
| Back-to-back, 20 ms timeout, `JOINT_POS` requested | 500 | 499/500 ok, RTT min=1614 avg=1685 max=1893 µs, stddev=25 µs |
| Back-to-back, 3 ms timeout (production value), `JOINT_POS` requested | 500 | 500/500 ok, RTT min=1376 avg=1432 max=1578 µs |
| Back-to-back, 2 ms timeout, `JOINT_POS` requested | 500 | 500/500 ok — zero loss |
| Back-to-back, 1 ms timeout, `JOINT_POS` requested | 300 | 0/300 ok — below the physical floor |
| Sustained burst, 20 ms timeout, `JOINT_POS` requested | 2000 | 2000/2000 ok, RTT min=1380 avg=1431 max=1527 µs, stddev=12 µs |
| Sustained burst, 20 ms timeout, no `JOINT_POS` (shorter response) | 2000 | 2000/2000 ok, RTT min=975 avg=1032 max=1157 µs, stddev=10 µs |

**Findings:**

- Real single-leg RTT (~1.0–1.4 ms typical) is noticeably higher than the
  theoretical ~710 µs/leg estimate in the table above — DE turnaround and
  RP2040 processing cost more in practice than the flat 150 µs/40 µs budgeted.
  Requesting `JOINT_POS` adds ~400 µs (longer response frame), matching the
  extra ~15 bytes on the wire.
- Extrapolated 6-leg sweep (sequential polling, as `hex_rs485_master` does):
  **~6.2 ms** without joint positions, **~8.6 ms** if every leg's poll
  requests positions every cycle — both inside the 10 ms / 100 Hz budget, but
  the all-positions case leaves only ~1.4 ms of slack. Since production only
  requests positions one-shot (not every cycle), realistic sustained sweep
  time is close to the faster figure.
- The production 3 ms per-leg response timeout has real margin: observed
  worst-case RTT topped out at ~1.9 ms even under continuous back-to-back
  load, no artificial gaps.
- ~1.0–1.3 ms is a hard physical floor (wire time + DE turnaround + RP2040
  processing) — sub-millisecond timeouts fail 100% of the time, not a
  software limitation.
- Bus jitter is tight (stddev 10–25 µs) once the link is healthy — see the
  firmware note below.
- Because the RS485 master task runs independently of the 100 Hz motion loop
  (non-blocking `rs485_master_get_telemetry()`), the motion loop can see leg
  telemetry up to one full sweep old in the worst case (~one loop late, not
  multiple) — an accepted trade-off for the non-blocking design.

**Firmware bug found and fixed during this bring-up**: the LegBoard's
`rs485_send()` (`firmware/leg/src/rs485.cpp`) released the RS485 driver
pin (`RS485_DE`) without draining the UART RX FIFO afterward. A turnaround
glitch left a stray byte in the buffer, which `rs485_poll_line()` on the next
call consumed as a bogus empty line — eating that cycle's real pull frame and
causing a clean, deterministic ~50% response loss (every other poll timed
out). Fixed by draining the RX FIFO immediately after releasing DE.

---

## Related Docs

- [`../architecture/SYSTEM_ARCHITECTURE.md`](../architecture/SYSTEM_ARCHITECTURE.md)
- [`../architecture/HARDWARE_AND_MECHANICS.md`](../architecture/HARDWARE_AND_MECHANICS.md)
- [`../../hardware/README.md`](../../hardware/README.md)
