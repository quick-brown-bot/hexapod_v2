// Compile-time configuration for the RP2040 LegBoard firmware.
//
// Pin map is from docs/architecture/HARDWARE_AND_MECHANICS.md.
// The "Pn" labels in that table are RP2040 GPIO numbers; "D4" is the XIAO
// silkscreen label for GP6. VERIFY against the LegBoard schematic before
// trusting on hardware.

#ifndef HEX_LEG_CONFIG_H
#define HEX_LEG_CONFIG_H

#include <stdint.h>

// --- Joints --------------------------------------------------------------
#define NUM_JOINTS 3
enum { JOINT_COXA = 0, JOINT_FEMUR = 1, JOINT_TIBIA = 2 };

// --- RP2040 GPIO assignments (XIAO RP2040) -------------------------------
// RS485 / UART0
#define PIN_UART_TX   0   // GP0  -> SP3485 DI
#define PIN_UART_RX   1   // GP1  <- SP3485 RO
#define PIN_RS485_DE  6   // GP6 (XIAO D4) -> SP3485 DE/RE direction control

// Servo PWM
#define PIN_PWM_COXA   3  // GP3
#define PIN_PWM_FEMUR  4  // GP4
#define PIN_PWM_TIBIA  2  // GP2

// Current sense (ADC). RP2040 ADC inputs: GP26=ADC0 .. GP29=ADC3.
#define PIN_ISENSE_TOTAL  26  // A0 / ADC0
#define PIN_ISENSE_COXA   27  // A1 / ADC1
#define PIN_ISENSE_FEMUR  28  // A2 / ADC2
#define PIN_ISENSE_TIBIA  29  // A3 / ADC3
#define ADC_CH_TOTAL  0
#define ADC_CH_COXA   1
#define ADC_CH_FEMUR  2
#define ADC_CH_TIBIA  3

// --- RS485 link ----------------------------------------------------------
#define RS485_BAUD            1000000UL
#define UART_INSTANCE_INDEX   0          // uart0 (TX=GP0, RX=GP1)

// --- Control loop --------------------------------------------------------
// Internal interpolation / servo update rate (independent of the RS485 rate).
// HARDWARE_AND_MECHANICS.md specifies 500-1000 Hz.
#define CONTROL_RATE_HZ   1000U
#define CONTROL_PERIOD_US (1000000U / CONTROL_RATE_HZ)

// --- Servo PWM -----------------------------------------------------------
#define SERVO_PWM_FREQ_HZ   50
#define SERVO_PERIOD_US     20000   // 1 / 50 Hz

// --- Stored parameter defaults (see RS485_PROTOCOL.md "Stored Parameters")
// Runtime params live in RAM at these defaults; the ESP32 re-applies any
// non-default values on recovery. They are NOT persisted to flash.
#define DEFAULT_MOVE_DURATION_MS     10
#define DEFAULT_WATCHDOG_TIMEOUT_MS  500
#define INTERP_MODE_LINEAR  0
#define INTERP_MODE_CUBIC   1
#define DEFAULT_INTERP_MODE  INTERP_MODE_LINEAR  // prove LINEAR first; CUBIC via P03

// Joint hard-limit defaults (degrees). Wide enough to allow full intended
// range until calibration narrows them. Master sends limits as 0.1-deg units
// via params P04..P09.
#define DEFAULT_JOINT_MIN_DEG  -90.0f
#define DEFAULT_JOINT_MAX_DEG   90.0f

// --- Servo calibration defaults (per servo) ------------------------------
// Calibration lives on the leg (the ESP32 cannot supply PWM calibration).
// Compile-time defaults now; shaped for flash persistence later.
#define DEFAULT_PWM_MIN_US      1000
#define DEFAULT_PWM_NEUTRAL_US  1500
#define DEFAULT_PWM_MAX_US      2000
#define DEFAULT_ANGLE_MIN_DEG   -90.0f
#define DEFAULT_ANGLE_MAX_DEG    90.0f

// --- Current sensing -----------------------------------------------------
#define ADC_RESOLUTION_BITS  12
#define ADC_MAX_COUNT        ((1 << ADC_RESOLUTION_BITS) - 1)
#define ADC_VREF_MV          3300
// INA4181 gain and shunt are board-specific; placeholder scale converts ADC
// volts to milliamps. Tune against measured load during bring-up.
#define ISENSE_MA_PER_MV     1.0f

// --- Flash persistence ---------------------------------------------------
#define PERSIST_MAGIC    0x4C454731UL  // "LEG1"
#define PERSIST_VERSION  1
#define DEFAULT_LEG_ADDR 1             // overwritten by calibration

#endif // HEX_LEG_CONFIG_H
