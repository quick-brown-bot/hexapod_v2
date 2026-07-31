"""Hexapod V2 — MainPowerBoard schematic (source of truth).

Owns all high-current energy distribution. Servo current must NEVER flow through
the MainBoard; it flows battery -> protection -> UBECs -> here -> LegBoards.

Contents:
* J_BATT  battery input (solder pads for cable) -> fuse -> master switch -> reverse-
  polarity protection -> +VMAIN distribution rail.
* J_UBEC1/2/3  servo BEC module headers (battery in, +6V out). UBEC#1 -> legs 1-2,
  UBEC#2 -> legs 3-4, UBEC#3 -> legs 5-6 (three independent VSERVO rails).
* J_SBEC  logic BEC module header (battery in, +5V out) -> MainBoard.
* J_LEG1..6  servo-power outputs to the six LegBoards.
* J_LOGIC  +5V logic output to the MainBoard.
* C1..C4  bulk capacitance (per-UBEC + battery rail) for transient servo loads.
* C5  optional soft-start capacitor on `PMOS_GATE_DRV`.
* D2..D5  power-present status LEDs (UBEC1/2/3 + SBEC).
* D6  15V Zener between PMOS gate and source rails (Vgs clamp).
* Q1/Q2  parallel IRF9540NPBF PMOS reverse-polarity stage (shared gate/rail nodes).
* Q3 + R5/R6/R7/R8  low-current gate driver for Q1/Q2 (NPN + gate network).
* J_MON   monitoring header: battery + per-UBEC voltages to MainBoard telemetry.

Protection path: F1 main fuse, then two parallel IRF9540NPBF PMOS devices (Q1/Q2)
as high-side reverse-polarity protection from `BATT_FUSED` to `+VMAIN`.
SW1 is a low-current control switch driving Q1/Q2 gates through Q3 + R5/R6/R7.
R8 fixes Q3 base state when SW1 is open; D6 clamps Vgs transients.

Run `python hardware/powerboard/powerboard_sch.py` to (re)generate
`powerboard.kicad_sch`. Connectivity is by net label.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))  # hardware
from _common import (  # noqa: E402
    BATTERY_SOLDER_PADS_2P_FOOTPRINT,
    BULK_CAP_1000UF_FOOTPRINT,
    BULK_CAP_2200UF_FOOTPRINT,
    CAP_0805_FOOTPRINT,
    DIODE_0805_FOOTPRINT,
    HEADER_1X05_THT_FOOTPRINT,
    HEADER_1X02_THT_FOOTPRINT,
    LED_0805_FOOTPRINT,
    NPN_SOT23_FOOTPRINT,
    PMOS_TO220_FOOTPRINT,
    RES_0805_FOOTPRINT,
    SCREW_TERMINAL_2P_FOOTPRINT,
    FUSE_20A_FOOTPRINT,
    SOLDER_PADS_4P_FOOTPRINT,
    MASTER_SWITCH_FOOTPRINT,
    imp,
    power_flag,
)

from hardware.schematic import Schematic, UuidRegistry  # noqa: E402

HERE = Path(__file__).resolve().parent

# UBEC -> leg allocation and the VSERVO rail each leg pair runs on.
LEG_RAIL = {1: "VSERVO1", 2: "VSERVO1", 3: "VSERVO2",
            4: "VSERVO2", 5: "VSERVO3", 6: "VSERVO3"}


def build() -> Schematic:
    reg = UuidRegistry(HERE / "uuids.json")
    sch = Schematic.new("powerboard", reg, paper="A4")

    imp(sch,
        "Connector:Screw_Terminal_01x02",
        "Connector:Conn_01x02_Pin",
        "Connector:Conn_01x03_Pin",
        "Connector:Conn_01x04_Pin",
        "Connector:Conn_01x05_Pin",
        "Device:Fuse",
        "Transistor_FET:Q_PMOS_GDS",
        "SymbolsLib:MMBT3904",
        "Device:R_Small",
        "Device:C_Small",
        "Device:C_Polarized",
        "Device:D_Zener",
        "Device:LED_Small")

    # Logical vertical placement grid.
    # User-specified spacing was in mm; schematic coordinates need a smaller scale
    # to stay on-sheet and readable.
    MM_TO_SCH = 0.05
    x_offset = 700 * MM_TO_SCH
    x_delta = 700 * MM_TO_SCH
    x1, x2, x3, x4, x5, x6, x7 = (x_offset + i * x_delta for i in range(7))
    y_delta = 300 * MM_TO_SCH
    y0 = 700 * MM_TO_SCH
    y1 = y0 + y_delta
    y2 = y0 + 2 * y_delta
    y3 = y0 + 3 * y_delta
    y4 = y0 + 4 * y_delta
    y5 = y0 + 5 * y_delta
    y6 = y0 + 6 * y_delta
    y7 = y0 + 7 * y_delta
    y8 = y0 + 8 * y_delta
    y9 = y0 + 9 * y_delta
    y10 = y0 + 10 * y_delta
    y11 = y0 + 11 * y_delta
    y12 = y0 + 12 * y_delta

    # --- battery input & protection chain -------------------------------- #
    j_bat = sch.place("Connector:Screw_Terminal_01x02", "J7", at=(x1, y0),
                      value="BATTERY",
                      footprint=BATTERY_SOLDER_PADS_2P_FOOTPRINT)
    sch.net("BATT_RAW", [j_bat.pin("1")], rotation=180)
    sch.net("GND", [j_bat.pin("2")], rotation=180)

    f1 = sch.place("Device:Fuse", "F1", at=(x1, y1), value="3413.0331.22 20A",
                   footprint=FUSE_20A_FOOTPRINT)
    sch.net("BATT_RAW", [f1.pin("1")])
    sch.net("BATT_FUSED", [f1.pin("2")])

    sw1 = sch.place("Connector:Conn_01x03_Pin", "SW1", at=(x1, y2),
                    value="MASTER_SW", footprint=MASTER_SWITCH_FOOTPRINT)
    sch.net("BATT_FUSED", [sw1.pin("1")])
    sch.net("GND", [sw1.pin("3")])

    # Parallel PMOS reverse-polarity stage (shared source/drain/gate nets).
    # Gate drive is low-current: SW1 -> R7 -> Q3, plus R6 pull-up and R5 gate resistor.
    q1 = sch.place("Transistor_FET:Q_PMOS_GDS", "Q1", at=(x1, y3), value="IRF9540NPBF",
                   footprint=PMOS_TO220_FOOTPRINT)
    q2 = sch.place("Transistor_FET:Q_PMOS_GDS", "Q2", at=(x1, y4), value="IRF9540NPBF",
                   footprint=PMOS_TO220_FOOTPRINT)
    sch.net("BATT_FUSED", [q1.pin("S"), q2.pin("S")])
    sch.net("+VMAIN", [q1.pin("D"), q2.pin("D")])

    # PMOS gate network: 100k pull-up to source rail and 470R series to gate bus.
    # 470R (not lower) keeps D6's continuous zener current low at full 4S charge --
    # see D6 comment below.
    r5 = sch.place("Device:R_Small", "R5", at=(x1, y5), value="470",
                   footprint=RES_0805_FOOTPRINT)
    r6 = sch.place("Device:R_Small", "R6", at=(x1, y6), value="100k",
                   footprint=RES_0805_FOOTPRINT)
    sch.net("PMOS_GATE_DRV", [r5.pin("1"), r6.pin("2")])
    sch.net("PMOS_GATE", [q1.pin("G"), q2.pin("G")], rotation=180)
    sch.net("PMOS_GATE", [r5.pin("2")])
    sch.net("BATT_FUSED", [r6.pin("1")])

    # NPN pull-down driver: collector to PMOS gate driver, emitter to GND,
    # base via 10k from switch output.
    q3 = sch.place("SymbolsLib:MMBT3904", "Q3", at=(x1, y7), value="MMBT3904",
                   footprint=NPN_SOT23_FOOTPRINT)
    r7 = sch.place("Device:R_Small", "R7", at=(x1, y8), value="10k",
                   footprint=RES_0805_FOOTPRINT)
    r8 = sch.place("Device:R_Small", "R8", at=(x1, y9), value="100k",
                   footprint=RES_0805_FOOTPRINT)
    sch.net("PMOS_GATE_DRV", [q3.pin("3")])
    sch.net("GND", [q3.pin("2")])
    sch.net("SW_CTRL", [sw1.pin("2"), r7.pin("1")])
    sch.net("NPN_BASE", [q3.pin("1")], rotation=180)
    sch.net("NPN_BASE", [r7.pin("2"), r8.pin("1")])
    sch.net("GND", [r8.pin("2")])

    # PMOS gate protection + optional inrush soft-start.
    d6 = sch.place("Device:D_Zener", "D6", at=(x1, y10), value="15V",
                   footprint=DIODE_0805_FOOTPRINT)
    c5 = sch.place("Device:C_Small", "C5", at=(x1, y11), value="100nF",
                   footprint=CAP_0805_FOOTPRINT)
    sch.net("BATT_FUSED", [d6.pin("1")], rotation=180)
    sch.net("PMOS_GATE", [d6.pin("2")])
    sch.net("PMOS_GATE_DRV", [c5.pin("1")])
    sch.net("GND", [c5.pin("2")])

    c4 = sch.place("Device:C_Polarized", "C4", at=(x1, y12), value="1000uF",
                   footprint=BULK_CAP_1000UF_FOOTPRINT)
    sch.net("+VMAIN", [c4.pin("1")])
    sch.net("GND", [c4.pin("2")])

    # --- servo UBEC modules (battery in -> VSERVOx out) ------------------ #
    for n, x in ((1, x2), (2, x3), (3, x4)):
        ju = sch.place("Connector:Conn_01x04_Pin", f"J{7 + n}", at=(x, y0),
                       value=f"UBEC{n}", footprint=SOLDER_PADS_4P_FOOTPRINT)
        rail = f"VSERVO{n}"
        sch.net("+VMAIN", [ju.pin("1")])   # battery in +
        sch.net("GND", [ju.pin("2")])
        sch.net(rail, [ju.pin("3")])       # regulated servo out
        sch.net("GND", [ju.pin("4")])
        # bulk capacitor on each servo rail
        c = sch.place("Device:C_Polarized", f"C{n}", at=(x, y3),
                      value="2200uF", footprint=BULK_CAP_2200UF_FOOTPRINT)
        sch.net(rail, [c.pin("1")])
        sch.net("GND", [c.pin("2")])
        # power-present status LED
        rl = sch.place("Device:R_Small", f"R{n}", at=(x, y1), value="1k",
                       footprint=RES_0805_FOOTPRINT)
        dl = sch.place("Device:LED_Small", f"D{1 + n}", at=(x, y2),
                       value=f"UBEC{n}", rotation=90,
                       footprint=LED_0805_FOOTPRINT)
        sch.net(rail, [rl.pin("1")], rotation=180)
        sch.net(f"LED_U{n}", [rl.pin("2"), dl.pin("2")])
        sch.net("GND", [dl.pin("1")])

    # --- logic SBEC module (battery in -> +5V out) ----------------------- #
    j_sbec = sch.place("Connector:Conn_01x04_Pin", "J11", at=(x5, y0),
                       value="SBEC", footprint=SOLDER_PADS_4P_FOOTPRINT)
    sch.net("+VMAIN", [j_sbec.pin("1")])
    sch.net("GND", [j_sbec.pin("2")])
    sch.net("+5V", [j_sbec.pin("3")])
    sch.net("GND", [j_sbec.pin("4")])
    r4 = sch.place("Device:R_Small", "R4", at=(x5, y1), value="1k",
                   footprint=RES_0805_FOOTPRINT)
    d5 = sch.place("Device:LED_Small", "D5", at=(x5, y2), value="SBEC",
                   rotation=90, footprint=LED_0805_FOOTPRINT)
    sch.net("+5V", [r4.pin("1")])
    sch.net("LED_SBEC", [r4.pin("2"), d5.pin("2")])
    sch.net("GND", [d5.pin("1")])

    # --- logic output to MainBoard --------------------------------------- #
    j_logic = sch.place("Connector:Conn_01x02_Pin", "J12", at=(x6, y7),
                        value="LOGIC_5V_OUT", footprint=HEADER_1X02_THT_FOOTPRINT)
    sch.net("+5V", [j_logic.pin("1")])
    sch.net("GND", [j_logic.pin("2")])

    # --- servo-power outputs to the six legs ----------------------------- #
    for leg in range(1, 7):
        jl = sch.place("Connector:Conn_01x02_Pin", f"J{leg}", at=(x6, y0 + (leg - 1) * 300 * MM_TO_SCH),
                       value=f"LEG{leg}_PWR",
                       footprint=SCREW_TERMINAL_2P_FOOTPRINT)
        sch.net(LEG_RAIL[leg], [jl.pin("1")])
        sch.net("GND", [jl.pin("2")])

    # --- monitoring header ----------------------------------------------- #
    j_mon = sch.place("Connector:Conn_01x05_Pin", "J13", at=(x6, y6),
                      value="MONITOR", footprint=HEADER_1X05_THT_FOOTPRINT)
    sch.net("+VMAIN", [j_mon.pin("1")])
    sch.net("VSERVO1", [j_mon.pin("2")])
    sch.net("VSERVO2", [j_mon.pin("3")])
    sch.net("VSERVO3", [j_mon.pin("4")])
    sch.net("GND", [j_mon.pin("5")])

    # --- ERC power flags (battery source + module outputs) --------------- #
    power_flag(sch, "#FLG1", (x7, y0), "BATT_RAW")
    power_flag(sch, "#FLG2", (x7, y1), "VSERVO1")
    power_flag(sch, "#FLG3", (x7, y2), "VSERVO2")
    power_flag(sch, "#FLG4", (x7, y3), "VSERVO3")
    power_flag(sch, "#FLG5", (x7, y4), "+5V")
    power_flag(sch, "#FLG6", (x7, y5), "+VMAIN")
    power_flag(sch, "#FLG7", (x7, y6), "GND")

    return sch


if __name__ == "__main__":
    build().write(HERE / "powerboard.kicad_sch")
    print("wrote powerboard.kicad_sch")
