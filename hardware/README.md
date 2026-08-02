# Hexapod boards

Schematics for the [system architecture](../docs/architecture/SYSTEM_ARCHITECTURE.md):
distributed leg controllers with logic power separated from servo power.

Three boards:

| Board | Source | Role |
|-------|--------|------|
| [MainBoard](mainboard/) | [`mainboard_sch.py`](mainboard/mainboard_sch.py) | ESP32 + IMU + RS485 master + 6× RJ11. Logic & comms only. |
| [LegBoard](legboard/) | [`legboard_sch.py`](legboard/legboard_sch.py) | XIAO RP2040 + SP3485 + 3 servos + sensing. One per leg. |
| [MainPowerBoard](powerboard/) | [`powerboard_sch.py`](powerboard/powerboard_sch.py) | Battery → fuse/switch/protection → 3 UBECs + SBEC → legs. |

There's also one bench-only tool, not part of the robot itself:

| Board | Source | Role |
|-------|--------|------|
| [calboard](calboard/) | [`calboard_sch.py`](calboard/calboard_sch.py) | XIAO RP2040 + resistor ladder. Plugs into a LegBoard's servo connectors in place of real servos to calibrate its current sensing — see [`firmware/calboard/README.md`](../firmware/calboard/README.md) and [`tools/current_calibration/`](../tools/current_calibration/). |

These schematics are authored **as code** with the
[`hardware-schematics`](schematic/README.md) toolchain. The Python script is
the source of truth; `uuids.json` is the committed UUID registry that protects
the PCB layout across regenerations. Edit the `.py`, never the `.kicad_sch` by
hand.

## Regenerating

```bash
python hardware/mainboard/mainboard_sch.py
python hardware/legboard/legboard_sch.py
python hardware/powerboard/powerboard_sch.py
```

Each writes its `<board>.kicad_sch` and updates `<board>/uuids.json` (additions
only for unchanged parts). `hardware/_common.py` resolves symbol libraries:
stock KiCad libs from the install (override with `KICAD_SYMBOL_DIR`), plus the
in-repo libraries under [`symbols/`](symbols/) — the Seeed XIAO series
(`Seeed_Studio_XIAO_Series.kicad_sym`), the ESP32 module (`SymbolsLib.kicad_sym`),
and `Hexapod_V2.kicad_sym` (flattened standalone `RJ25` and `SP3485CN`, because
the stock library defines those as derived `(extends …)` symbols the code
importer cannot follow).

The boards are self-contained: symbol libraries, footprints, and the FreeCAD
model live here under [`symbols/`](symbols/), [`footprints/`](footprints/) and
[`models/`](models/); they do not depend on any shared top-level libraries.

## Inter-board connections

```text
MainPowerBoard.J_LOGIC (+5V) ──► MainBoard.J7 (SBEC_5V_IN)
MainBoard.J1..J6 (RJ11)      ──► LegBoard.J1 (UPLINK_RJ11)        ×6
MainPowerBoard.J1..J6 (+6V)  ──► LegBoard.J5 (SERVO_PWR_IN)       ×6
```

### RJ11 / RJ25 leg link (MainBoard ↔ LegBoard)

The physical cable is populated as a 4-wire link inside a 6P6C/RJ25 shell, so
pins 1 and 6 are intentionally left unconnected.

| Pin | Net | Function |
|-----|-----|----------|
| 1 | — | unused |
| 2 | GND | Ground |
| 3 | RS485_A | RS485 A |
| 4 | RS485_B | RS485 B |
| 5 | +5V | Logic power |
| 6 | — | unused |

## MainBoard nets

| Net | Connects |
|-----|----------|
| `+5V` | SBEC input (J7), ESP32 dev-board VIN, all six RJ11 pin 5 |
| `+3.3V` | ESP32 dev-board 3V3 output, SP3485 VCC, IMU, decoupling, EN pull-up |
| `GND` | global ground |
| `ESP_TXD` / `ESP_RXD` | ESP32 IO17/IO16 (UART2) ↔ SP3485 DI/RO |
| `RS485_DE` | ESP32 IO4 → SP3485 DE + ~RE (direction) |
| `RS485_A` / `RS485_B` | SP3485 A/B ↔ all six RJ11, 120 Ω term (R1) |
| `IMU_SDA` / `IMU_SCL` / `IMU_INT` | ESP32 IO21/IO22/IO34 ↔ IMU header (J9) |

MainBoard decoupling intent (implemented in schematic code): `C1` 10 uF on +5V at
power entry (close to J7/U1 VIN), `C2` 100 nF at ESP32 3V3 pins, `C3` 100 nF at
SP3485 VCC, `C4` 100 nF at IMU header power pins, and `C5` 10 uF bulk on +3.3V
near the ESP32 3V3-source/load hub.

IMU header J9 (`Conn_01x05`): 1 = +3.3V, 2 = GND, 3 = SDA, 4 = SCL, 5 = INT.

Footprints currently driven from the generator: `U1` uses
`Custom:ESP32_Dev_Board`, `J1..J6` use
`Connector_RJ:RJ25_Wayconn_MJEA-660X1_Horizontal`, `J9` uses
`Connector_PinHeader_2.54mm:PinHeader_1x05_P2.54mm_Vertical`, `U2` uses the
SOIC-8 SP3485 footprint, and the ordinary small R/C parts default to 0805.

## LegBoard nets

| Net | Connects |
|-----|----------|
| `+5V` | RJ11 pin 5 logic power → XIAO VBUS (U1.14) |
| `+3.3V` | XIAO 3V3 out (U1.12) → SP3485 VCC, INA4181 VCC, decoupling |
| `+6V` | post-total-shunt servo bus; high-side **force** node feeding the three per-servo branch shunts and bulk cap C2 |
| `+6V_IN` | raw servo-power input (J5) ahead of the total shunt |
| `+6V_COXA` / `+6V_FEMUR` / `+6V_TIBIA` | post-branch-shunt servo supply rails (force) for J2 / J3 / J4 |
| `ITOT_SP` / `ITOT_SN` | Kelvin **sense** taps of total shunt R4 → INA IN+1 / IN-1 |
| `COXA_SP` / `COXA_SN` | Kelvin sense taps of coxa shunt R5 → INA IN+4 / IN-4 |
| `FEMUR_SP` / `FEMUR_SN` | Kelvin sense taps of femur shunt R3 → INA IN+2 / IN-2 |
| `TIBIA_SP` / `TIBIA_SN` | Kelvin sense taps of tibia shunt R2 → INA IN+3 / IN-3 |
| `GND` | global ground |
| `RP_TXD` / `RP_RXD` | XIAO P0/P1 (UART) ↔ SP3485 DI/RO |
| `RS485_DE` | XIAO P6/D4 → SP3485 DE + ~RE |
| `RS485_A` / `RS485_B` | SP3485 ↔ RJ11, 120 Ω term R1 (DNP except bus ends) |
| `COXA_PWM` / `FEMUR_PWM` / `TIBIA_PWM` | XIAO P3/P4/P2 → servo connectors J2/J3/J4 |
| `I_TOTAL_SENSE` | INA4181 channel 1 output → XIAO A0 |
| `I_FEMUR_SENSE` | INA4181 channel 2 output → XIAO A1 |
| `I_TIBIA_SENSE` | INA4181 channel 3 output → XIAO A2 |
| `I_COXA_SENSE` | INA4181 channel 4 output → XIAO A3 |

Servo connectors J2/J3/J4 (`Conn_01x03`): 1 = signal, 2 = branch +6V, 3 = GND.
J5 is a 2-pin screw terminal for servo power input. The four current shunts
`R2`/`R3`/`R4`/`R5` are **4-terminal (Kelvin) shunts** (`Device:R_Shunt`): pins
1/4 carry current, pins 2/3 are the sense taps. Each INA input reaches the shunt
over its own unique two-node `*_SP`/`*_SN` net, so the autorouter cannot merge
the three high-side taps (INA pins 6/15/17) through the shared `+6V` copper or
assign them current-rail trace widths. U3 (`INA4181A3IPWR`, gain 100) measures channel 1 across the total-leg shunt
`R4` (`+6V_IN`→`+6V`), channel 2 across femur `R3`, channel 3 across tibia
`R2`, channel 4 across coxa `R5`; IN+ is always on the higher-potential
(source) side. The generated sheet layout keeps the XIAO and INA
blocks as top-level anchors with the connector and power groups below for
readability.

> **Shunt placement must stay at rotation 0.** The schematic-as-code pin
> transform mismatches KiCad for off-axis pins at rotation 90/270 — the Kelvin
> sense pads land off the pin and the force pads swap. Verified via netlist
> export; keep the `Device:R_Shunt` placements un-rotated.

Footprints currently driven from the generator: `U1` uses
`Seeed_Studio_XIAO_Series:XIAO-RP2040-DIP`, `J1` uses
`Connector_RJ:RJ25_Wayconn_MJEA-660X1_Horizontal`, `U2` uses the SOIC-8
SP3485 footprint, `U3` uses `Custom:PW20_TEX`, `J2`/`J3`/`J4` use
`Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical`, `J5` uses
`TerminalBlock:TerminalBlock_MaiXu_MX126-5.0-02P_1x02_P5.00mm`, the four INA
shunts now use the 4-contact Kelvin footprint
`Resistor_SMD:R_Shunt_Ohmite_LVK12`, `C2` uses the through-hole radial candidate
`Capacitor_THT:CP_Radial_D8.0mm_P3.50mm` for an 8 mm diameter bulk capacitor, and
the ordinary small R/C parts default to 0805. Verify the LVK12 shunt power rating
and the capacitor lead pitch / body height against the exact parts before
freezing the PCB.

## MainPowerBoard nets

| Net | Connects |
|-----|----------|
| `BATT_RAW`→`BATT_FUSED`→`+VMAIN` | battery → F1 fuse → Q1/Q2 parallel PMOS reverse-polarity stage → main rail |
| `+VMAIN` | feeds the three UBEC modules (J8/J9/J10), the SBEC (J11), monitor (J13), bulk C4 |
| `VSERVO1` | UBEC1 out → legs 1,2 (J1,J2), bulk C1, LED D2 |
| `VSERVO2` | UBEC2 out → legs 3,4 (J3,J4), bulk C2, LED D3 |
| `VSERVO3` | UBEC3 out → legs 5,6 (J5,J6), bulk C3, LED D4 |
| `+5V` | SBEC out (J11) → logic output (J12), LED D5 |
| `GND` | global ground |

UBEC/SBEC module headers (`Conn_01x04`): 1 = +VMAIN in, 2 = GND, 3 = regulated
out, 4 = GND. Monitor header J13 (`Conn_01x05`): +VMAIN, VSERVO1, VSERVO2,
VSERVO3, GND.

`F1` is a soldered-in (non-replaceable) SMD fuse, Schurter 3413.0331.22
(20A, time-lag, 1206/`Fuse:Fuse_1206_3216Metric`) — chosen over an automotive
mini-blade holder because TME stocks no SMD blade-fuse holder; the previous
`Custom:Fuse Holder Mini` THT holder footprint is unused now but left in the
library.

Footprints currently driven from the generator: `J7` (battery input) uses
`Connector_Wire:SolderWire-1.5sqmm_1x02_P7.8mm_D1.7mm_OD3.9mm` solder pads,
`J1..J6` use
`TerminalBlock:TerminalBlock_MaiXu_MX126-5.0-02P_1x02_P5.00mm`, and `J13` uses
`Connector_PinHeader_2.54mm:PinHeader_1x05_P2.54mm_Vertical`. Bulk caps use
`C1..C3`: `Capacitor_THT:CP_Radial_D13.0mm_P5.00mm` (2200 uF class) and `C4`:
`Capacitor_THT:CP_Radial_D8.0mm_P3.80mm` (1000 uF class). The status resistor /
LED parts default to 0805. Reverse-polarity protection uses two parallel
`FQP47P06` PMOS parts in `Package_TO_SOT_THT:TO-220-3_Vertical`.

Power-entry control is now low-current at SW1: the high-current path is
`BATT_FUSED -> Q1/Q2 -> +VMAIN`, while SW1 only drives the PMOS gates through
`R7=10k` + `Q3` (NPN pull-down), with `R6=100k` pull-up and `R5=100R` series
gate resistor for damping.

Pre-prototype hardening now added in-code:
- `R8=100k` from `NPN_BASE` to `GND` (base pull-down; avoids floating base when SW1 is open).
- `D6=15V` Zener between `PMOS_GATE` and `BATT_FUSED` (Vgs transient clamp).
- Optional `C5=100nF` from `PMOS_GATE_DRV` to `GND` (slows turn-on edge to soften inrush into ~7600 uF total bulk capacitance).

## PCB order tagging

Each board order placed with JLCPCB is tagged in git at the exact commit that
was sent to manufacture:

```
git tag pcb_order_<JLCPCB-order-ID> <commit>
```

Example: `pcb_order_W2026062121528170` points to the first legboard/mainboard
order. To inspect or check out any ordered revision:

```bash
git show pcb_order_W2026062121528170          # view commit
git checkout pcb_order_W2026062121528170      # restore that state
```

This makes it possible to diff the current schematic against what was actually
fabricated, or to flash firmware that matches the physical board on hand.

---

## Status / known limitations (first pass)

These are deliberate module-abstraction simplifications to refine in Eeschema
before fabrication:

* **Reverse-polarity protection** now uses two parallel PMOS parts (`Q1`/`Q2`,
  `FQP47P06`) to leave thermal headroom for high current, plus a low-current
  gate-driver stage (`Q3`, `R5`, `R6`, `R7`, `R8`) so SW1 does not switch inrush
  current directly.
* **UBEC / SBEC / IMU / current sensor** are represented as module headers, not
  discrete designs.
* **ERC** loads cleanly but reports expected violations: unused ESP32/XIAO GPIO
  and the ESP32 `VIN`/`SENSOR_*` pins are left open (add no-connect flags), and
  off-grid endpoints (the loose auto-layout is meant to be re-placed in
  Eeschema). Net-label connectivity is verified correct via exported netlist.
* **Remaining footprint work** is limited to the high-current / bulk power-path
  parts and the placeholder module headers; the ordinary V2 passives, ESP32,
  XIAO RP2040, RJ25 connectors, and the IMU header are now assigned in code.
