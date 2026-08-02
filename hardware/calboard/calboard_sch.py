"""Hexapod V2 — current-sensor calibration adapter board (bench tool only).

NOT part of the robot. Plugs into a single LegBoard's servo connectors
(J2/J3/J4) in place of real servos, so the LegBoard's INA4181 current sensing
can be calibrated against a known load repeatably instead of by hand with a
multimeter. See firmware/calboard/README.md and tools/current_calibration/
for the full workflow this board is part of.

Contents:
* U1      XIAO RP2040 — decodes incoming servo PWM per channel, drives the
    resistor ladder via the shift registers, reads back rail voltage, talks
    to the host over USB.
* U2/U3   74HC595 shift registers, daisy-chained, drive all 12 ladder-leg
    MOSFET gates from 3 GPIO (SER/SRCLK/RCLK). U2 (closer to the MCU in the
    chain) ends up driving coxa (QA-QD) + femur (QE-QH); U3 drives tibia
    (QA-QD) with QE-QH unused. This chain-position -> channel assignment
    falls straight out of shiftOut() sending bit15 first / bit0 last from
    firmware/calboard/src/ladder.cpp's `s_bits` layout (bits0-3=coxa,
    4-7=femur, 8-11=tibia) — see that file's header comment before changing
    either side.
* Q1-Q12  Logic-level N-MOSFETs (Transistor_FET:Q_NMOS_GSD), one per ladder
    leg, switching each leg's resistor to GND. Gate driven by its shift
    register Q pin, drain through its resistor to the channel's +6V branch
    rail, source to GND.
* R1-R12  Binary-weighted ladder resistors, 4 per channel (bit0=strongest
    resistor/weakest conductance .. bit3=weakest resistor/strongest
    conductance) — values from firmware/calboard/src/config.h
    (LADDER_R1..R4_OHM). PLACEHOLDER VALUES, see that file's comment: bit0
    (220R) needs a 1206 footprint (0805 alone is underrated for its ~164mW
    at 6V), R3/R4 need real power resistors, not SMD, and the whole ladder
    needs reviewing against actual servo current draw before fab.
* R13-R18 Rail-voltage divider resistors (2 per channel, equal value for a
    0.5 ratio matching VSENSE_DIVIDER_RATIO in config.h) feeding the MCU ADC.
* J1/J2/J3 Servo-style 3-pin headers (coxa/femur/tibia): signal / +6V branch
    / GND — same connector part and pin order as LegBoard J2/J3/J4
    (hardware/legboard/legboard_sch.py) so the same cables mate directly.

Power model: USB VBUS powers the XIAO directly (this board has no RS485 link
and needs no external logic supply); its on-board regulator supplies +3.3V
for the shift registers. Each channel's +6V branch rail comes from the
LegBoard itself over the servo connector — this board sinks that leg's real
current through its own known resistors, so the LegBoard's own shunt/INA4181
sees genuine current to calibrate against.

Run `python hardware/calboard/calboard_sch.py` to (re)generate
`calboard.kicad_sch`. Connectivity is by net label.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))  # hardware
from _common import (  # noqa: E402
    HEADER_1X03_THT_FOOTPRINT,
    NMOS_SOT23_FOOTPRINT,
    RES_0805_FOOTPRINT,
    RES_1206_FOOTPRINT,
    RES_POWER_AXIAL_FOOTPRINT,
    SHIFT_REG_DIP16_FOOTPRINT,
    XIAO_RP2040_DIP_FOOTPRINT,
    imp,
    power_flag,
)

from hardware.schematic import Schematic, UuidRegistry  # noqa: E402

HERE = Path(__file__).resolve().parent

# Per-channel ladder-leg resistor values, weakest (bit0) to strongest (bit3)
# conductance, and which footprint each needs — see module docstring / R1-R12
# above. Matches LADDER_R1..R4_OHM in firmware/calboard/src/config.h.
LEG_OHMS = (220.0, 100.0, 47.0, 22.0)
LEG_FOOTPRINTS = (
    # 220Ohm dissipates ~164mW at 6V, over an 0805's usual 125mW rating with
    # no margin -- bump to 1206 (typ. 250mW) instead. TME: CRCW1206220RFKTABC.
    RES_1206_FOOTPRINT,
    RES_0805_FOOTPRINT,
    RES_POWER_AXIAL_FOOTPRINT,
    RES_POWER_AXIAL_FOOTPRINT,
)


def build() -> Schematic:
    reg = UuidRegistry(HERE / "uuids.json")
    sch = Schematic.new("calboard", reg, paper="A4")

    imp(sch,
        "Seeed_Studio_XIAO_Series:XIAO-RP2040-DIP",
        "74xx:74HC595",
        "Transistor_FET:Q_NMOS_GSD",
        "Connector:Conn_01x03_Pin",
        "Device:R_Small")

    # --- local controller --------------------------------------------------- #
    u1 = sch.place("Seeed_Studio_XIAO_Series:XIAO-RP2040-DIP", "U1", at=(70, 40),
                   value="XIAO-RP2040", footprint=XIAO_RP2040_DIP_FOOTPRINT)
    sch.net("+5V", [u1.pin("14")])       # VBUS <- USB (this board's only power source)
    sch.net("+3.3V", [u1.pin("12")])     # 3V3 OUT (on-board regulator)
    sch.net("GND", [u1.pin("13")])
    sch.net("PWMIN_COXA", [u1.pin("7")])    # P0/D6 <- LegBoard J2 pin1
    sch.net("PWMIN_FEMUR", [u1.pin("8")])   # P1/D7 <- LegBoard J3 pin1
    sch.net("PWMIN_TIBIA", [u1.pin("9")])   # P2/D8 <- LegBoard J4 pin1
    sch.net("VSENSE_COXA", [u1.pin("1")], rotation=180)   # P26/A0 (ADC)
    sch.net("VSENSE_FEMUR", [u1.pin("2")], rotation=180)  # P27/A1 (ADC)
    sch.net("VSENSE_TIBIA", [u1.pin("3")], rotation=180)  # P28/A2 (ADC)
    sch.net("SR_SER", [u1.pin("10")])    # P4/D9  -> both 74HC595 SER (chained)
    sch.net("SR_RCLK", [u1.pin("11")])   # P3/D10 -> both 74HC595 RCLK
    sch.net("SR_SRCLK", [u1.pin("5")])   # P6/D4  -> both 74HC595 SRCLK

    # --- shift registers (drive all 12 ladder-leg gates from 3 GPIO) -------- #
    # Pin numbers per standard 74HC595 DIP-16/SOIC-16 datasheet numbering —
    # VERIFY against the actual KiCad symbol once opened in Eeschema.
    u2 = sch.place("74xx:74HC595", "U2", at=(160, 30), value="74HC595",
                   footprint=SHIFT_REG_DIP16_FOOTPRINT)
    sch.net("SR_SER", [u2.pin("14")])
    sch.net("SR_SRCLK", [u2.pin("11")])
    sch.net("SR_RCLK", [u2.pin("12")])
    sch.net("+3.3V", [u2.pin("10"), u2.pin("16")])  # SRCLR (disabled), VCC
    sch.net("GND", [u2.pin("13"), u2.pin("8")])      # OE (enabled), GND
    sch.net("GATE_COXA_B0", [u2.pin("15")])   # QA
    sch.net("GATE_COXA_B1", [u2.pin("1")])    # QB
    sch.net("GATE_COXA_B2", [u2.pin("2")])    # QC
    sch.net("GATE_COXA_B3", [u2.pin("3")])    # QD
    sch.net("GATE_FEMUR_B0", [u2.pin("4")])   # QE
    sch.net("GATE_FEMUR_B1", [u2.pin("5")])   # QF
    sch.net("GATE_FEMUR_B2", [u2.pin("6")])   # QG
    sch.net("GATE_FEMUR_B3", [u2.pin("7")])   # QH
    sch.net("SR_CASCADE", [u2.pin("9")])      # QH' -> U3 SER

    u3 = sch.place("74xx:74HC595", "U3", at=(160, 80), value="74HC595",
                   footprint=SHIFT_REG_DIP16_FOOTPRINT)
    sch.net("SR_CASCADE", [u3.pin("14")])
    sch.net("SR_SRCLK", [u3.pin("11")])
    sch.net("SR_RCLK", [u3.pin("12")])
    sch.net("+3.3V", [u3.pin("10"), u3.pin("16")])
    sch.net("GND", [u3.pin("13"), u3.pin("8")])
    sch.net("GATE_TIBIA_B0", [u3.pin("15")])  # QA
    sch.net("GATE_TIBIA_B1", [u3.pin("1")])   # QB
    sch.net("GATE_TIBIA_B2", [u3.pin("2")])   # QC
    sch.net("GATE_TIBIA_B3", [u3.pin("3")])   # QD
    # QE-QH (pins 4-7) and QH' (pin 9): unused, left unconnected.

    # --- per-channel: servo connector, ladder legs, voltage divider -------- #
    r_ref = 1
    q_ref = 1
    div_ref = 13
    for ch_name, x, pwm_net, vsense_net, gate_prefix, j_ref in (
            ("coxa", 20, "PWMIN_COXA", "VSENSE_COXA", "GATE_COXA", "J1"),
            ("femur", 90, "PWMIN_FEMUR", "VSENSE_FEMUR", "GATE_FEMUR", "J2"),
            ("tibia", 160, "PWMIN_TIBIA", "VSENSE_TIBIA", "GATE_TIBIA", "J3")):
        rail = f"{ch_name.upper()}_RAIL"

        j = sch.place("Connector:Conn_01x03_Pin", j_ref, at=(x, 110),
                      value=ch_name.upper(), footprint=HEADER_1X03_THT_FOOTPRINT)
        sch.net(pwm_net, [j.pin("1")])
        sch.net(rail, [j.pin("2")])
        sch.net("GND", [j.pin("3")])

        # 4 binary-weighted legs: RAIL -> resistor -> MOSFET drain; MOSFET
        # source -> GND; gate <- shift register bit for this channel.
        for bit in range(4):
            r = sch.place("Device:R_Small", f"R{r_ref}", at=(x - 10 + bit * 6, 130),
                          value=f"{LEG_OHMS[bit]:g}", rotation=90,
                          footprint=LEG_FOOTPRINTS[bit])
            sch.net(rail, [r.pin("1")])
            leg_node = f"{ch_name.upper()}_LEG{bit}"
            sch.net(leg_node, [r.pin("2")])

            q = sch.place("Transistor_FET:Q_NMOS_GSD", f"Q{q_ref}",
                          at=(x - 10 + bit * 6, 145), value="Q_NMOS_GSD",
                          footprint=NMOS_SOT23_FOOTPRINT)
            sch.net(f"{gate_prefix}_B{bit}", [q.pin("1")])  # G
            sch.net("GND", [q.pin("2")])                     # S
            sch.net(leg_node, [q.pin("3")])                  # D
            r_ref += 1
            q_ref += 1

        # Voltage divider (equal legs, ratio 0.5 — matches
        # VSENSE_DIVIDER_RATIO in firmware/calboard/src/config.h).
        r_top = sch.place("Device:R_Small", f"R{div_ref}", at=(x + 20, 130),
                          value="10k", rotation=90, footprint=RES_0805_FOOTPRINT)
        sch.net(rail, [r_top.pin("1")])
        sch.net(vsense_net, [r_top.pin("2")])
        div_ref += 1
        r_bottom = sch.place("Device:R_Small", f"R{div_ref}", at=(x + 20, 145),
                             value="10k", rotation=90, footprint=RES_0805_FOOTPRINT)
        sch.net(vsense_net, [r_bottom.pin("1")])
        sch.net("GND", [r_bottom.pin("2")])
        div_ref += 1

    # --- ERC power flags ----------------------------------------------------- #
    power_flag(sch, "#FLG1", (35, 175), "+5V")
    power_flag(sch, "#FLG2", (55, 175), "+3.3V")
    power_flag(sch, "#FLG3", (75, 175), "GND")

    return sch


if __name__ == "__main__":
    build().write(HERE / "calboard.kicad_sch")
    print("wrote calboard.kicad_sch")
