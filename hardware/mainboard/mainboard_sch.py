"""Hexapod V2 — MainBoard schematic (source of truth).

Central logic + communication controller. Per the V2 architecture, this board
carries logic power, communication and control signals ONLY — never servo
current.

Contents:
* U1  ESP32-WROOM-32D — gait/IK/stabilisation/comms (the "intelligence").
* U2  SP3485CN RS485 transceiver — bus master to the six leg controllers.
* J7  SBEC logic-power input screw terminal from the MainPowerBoard (+5V).
* J_IMU  IMU breakout header (I2C + INT) — module abstraction, processed centrally.
* J1..J6 RJ11 (RJ25 6P used as 4-wire) leg interfaces: NC / GND / RS485 A /
    RS485 B / +5V logic / NC.
* C1  10uF THT bulk capacitor on +5V entry.
* C2  100nF local decoupling for ESP32 3V3.
* C3  100nF local decoupling for RS485 transceiver 3V3.
* C4  100nF local decoupling for IMU header 3V3.
* C5  10uF local bulk decoupling on +3.3V rail.

Power model: SBEC +5V comes in via screw terminal J7, is distributed unchanged
to the legs over RJ11 (logic power), and feeds the ESP32 dev board VIN. The
dev board's onboard regulator generates +3.3V, and that +3.3V pin powers the
IMU, RS485 transceiver and local decoupling network.

Run `python hardware/mainboard/mainboard_sch.py` to (re)generate
`mainboard.kicad_sch`. Connectivity is by net label; placement is a loose grid
meant to be tidied in Eeschema.

NOTE: unused ESP32 GPIO pins are left open here — add no-connect flags in
Eeschema before relying on ERC.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))  # hardware
from _common import (  # noqa: E402
    BULK_CAP_10UF_FOOTPRINT,
    CAP_0805_FOOTPRINT,
    ESP32_DEV_BOARD_FOOTPRINT,
    HEADER_1X05_THT_FOOTPRINT,
    RES_0805_FOOTPRINT,
    RJ25_FOOTPRINT,
    SCREW_TERMINAL_2P_FOOTPRINT,
    SP3485_SOIC8_FOOTPRINT,
    imp,
    power_flag,
)

from hardware.schematic import Schematic, UuidRegistry  # noqa: E402

HERE = Path(__file__).resolve().parent


def build() -> Schematic:
    reg = UuidRegistry(HERE / "uuids.json")
    sch = Schematic.new("mainboard", reg, paper="A4")

    imp(sch,
        "SymbolsLib:ESP32-WROOM-32D",
        "Hexapod_V2:SP3485CN",
        "Hexapod_V2:RJ25",
        "Connector:Conn_01x02_Pin",
        "Connector:Conn_01x05_Pin",
        "Device:R_Small",
        "Device:C_Small",
        "Device:C_Polarized")

    # --- main controller -------------------------------------------------- #
    u1 = sch.place("SymbolsLib:ESP32-WROOM-32D", "U1", at=(130, 55),
                   value="ESP32-WROOM-32D",
                   footprint=ESP32_DEV_BOARD_FOOTPRINT)
    sch.net("+5V", [u1.pin("2")])    # VIN
    sch.net("+3.3V", [u1.pin("V3.3")])
    sch.net("GND", [u1.pin("1")])
    sch.net("ESP_TXD", [u1.pin("IO17")])      # UART2 TX -> RS485 DI
    sch.net("ESP_RXD", [u1.pin("IO16")])      # UART2 RX <- RS485 RO
    sch.net("RS485_DE", [u1.pin("IO4")])      # driver enable / ~receiver enable
    sch.net("IMU_SDA", [u1.pin("IO21")])
    sch.net("IMU_SCL", [u1.pin("IO22")])
    sch.net("IMU_INT", [u1.pin("IO34")])

    # --- RS485 master ----------------------------------------------------- #
    u2 = sch.place("Hexapod_V2:SP3485CN", "U2", at=(195, 55), value="SP3485CN",
                   footprint=SP3485_SOIC8_FOOTPRINT)
    
    sch.net("+3.3V", [u2.pin("8")])     # VCC
    sch.net("GND", [u2.pin("5")])       # GND
    sch.net("ESP_RXD", [u2.pin("1")], rotation=180)                  # RO
    sch.net("ESP_TXD", [u2.pin("4")], rotation=180)                  # DI
    sch.net("RS485_DE", [u2.pin("2"), u2.pin("3")], rotation=180)    # ~RE + DE tied
    sch.net("RS485_A", [u2.pin("6")])
    sch.net("RS485_B", [u2.pin("7")])

    # Bus termination 100R across A/B (fit on the master end of the bus).
    rt = sch.place("Device:R_Small", "R1", at=(220, 95), value="100",
                   rotation=90, footprint=RES_0805_FOOTPRINT)
    sch.net("RS485_A", [rt.pin("1")])
    sch.net("RS485_B", [rt.pin("2")], rotation=180)

    # --- power input ----------------------------------------------------- #
    j_sbec = sch.place("Connector:Conn_01x02_Pin", "J7", at=(40, 90),
                       value="SBEC_5V_IN", footprint=SCREW_TERMINAL_2P_FOOTPRINT)
    sch.net("+5V", [j_sbec.pin("1")])
    sch.net("GND", [j_sbec.pin("2")])

    # --- IMU breakout (I2C) ---------------------------------------------- #
    j_imu = sch.place("Connector:Conn_01x05_Pin", "J9", at=(70, 120),
                      value="IMU_I2C", footprint=HEADER_1X05_THT_FOOTPRINT)
    sch.net("+3.3V", [j_imu.pin("1")])
    sch.net("GND", [j_imu.pin("2")])
    sch.net("IMU_SDA", [j_imu.pin("3")])
    sch.net("IMU_SCL", [j_imu.pin("4")])
    sch.net("IMU_INT", [j_imu.pin("5")])

    # --- decoupling ------------------------------------------------------- #
    # C1: place at the +5V entry cluster, physically close to J7 and U1 VIN.
    c1 = sch.place("Device:C_Polarized", "C1", at=(50, 110), value="10uF",
                   footprint=BULK_CAP_10UF_FOOTPRINT)
    sch.net("+5V", [c1.pin("1")])
    sch.net("GND", [c1.pin("2")])

    # C2: place immediately next to U1 3V3/GND pins (short return loop).
    c2 = sch.place("Device:C_Small", "C2", at=(150, 125), value="100nF",
                   footprint=CAP_0805_FOOTPRINT)
    sch.net("+3.3V", [c2.pin("1")])
    sch.net("GND", [c2.pin("2")])

    # C3: place immediately next to U2 VCC/GND pins.
    c3 = sch.place("Device:C_Small", "C3", at=(175, 125), value="100nF",
                   footprint=CAP_0805_FOOTPRINT)
    sch.net("+3.3V", [c3.pin("1")])
    sch.net("GND", [c3.pin("2")])

    # C4: place at the IMU header power pins (J9 pin 1/2).
    c4 = sch.place("Device:C_Small", "C4", at=(90, 125), value="100nF",
                   footprint=CAP_0805_FOOTPRINT)
    sch.net("+3.3V", [c4.pin("1")])
    sch.net("GND", [c4.pin("2")])

    # C5: place near the 3V3 source/load hub (between U1 3V3 and U2/IMU loads).
    c5 = sch.place("Device:C_Polarized", "C5", at=(115, 125), value="10uF",
                   footprint=BULK_CAP_10UF_FOOTPRINT)
    sch.net("+3.3V", [c5.pin("1")])
    sch.net("GND", [c5.pin("2")])

    # --- RJ11 leg interfaces (x6): pins 1 and 6 intentionally unused ----- #
    for i in range(1, 7):
        j = sch.place("Hexapod_V2:RJ25", f"J{i}", at=(40 + (i - 1) * 35, 155),
                      value=f"LEG{i}_RJ11", footprint=RJ25_FOOTPRINT)
        sch.net("GND", [j.pin("2")])
        sch.net("RS485_A", [j.pin("3")])
        sch.net("RS485_B", [j.pin("4")])
        sch.net("+5V", [j.pin("5")])

    # --- ERC power flags -------------------------------------------------- #
    power_flag(sch, "#FLG1", (40, 135), "+5V")
    power_flag(sch, "#FLG3", (80, 135), "GND")

    return sch


if __name__ == "__main__":
    build().write(HERE / "mainboard.kicad_sch")
    print("wrote mainboard.kicad_sch")
