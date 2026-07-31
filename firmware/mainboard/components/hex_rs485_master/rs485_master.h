#ifndef RS485_MASTER_H
#define RS485_MASTER_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "robot_config.h" // for NUM_LEGS

// =============================================================================
// hex_rs485_master — RS485 bus master for the V2 distributed leg architecture.
//
// Owns UART2 + the SP3485 transceiver exclusively. Runs its own FreeRTOS task
// that polls all six legs sequentially, independent of the 100 Hz motion loop.
//
// The motion loop NEVER blocks on this component:
//   - rs485_master_set_leg_angles() writes desired angles into a shared command
//     buffer and returns immediately.
//   - rs485_master_get_telemetry() returns the most recent telemetry snapshot,
//     which may be stale (flagged) if the leg has not responded recently.
//
// Protocol details (frame format, CRC, parameter IDs): see
//   docs/interfaces/RS485_PROTOCOL.md
//
// Angles are passed in DEGREES (matching the wire protocol). The motion
// pipeline works in radians; hex_actuation performs the rad->deg conversion
// before calling in. This component does no kinematic transform, calibration,
// offset, or PWM mapping — those live on the RP2040.
// =============================================================================

// Per-leg telemetry snapshot returned to the motion loop.
typedef struct {
    bool valid;             // false until the first successful response from this leg
    bool stale;             // true if the last transaction to this leg timed out / failed CRC
    uint8_t status;         // status flags echoed by the leg (see RS485_PROTOCOL.md)
    uint16_t current_total_ma;
    uint16_t current_coxa_ma;
    uint16_t current_femur_ma;
    uint16_t current_tibia_ma;
    bool has_positions;     // true if joint positions are present in this snapshot
    float pos_coxa_deg;     // valid only when has_positions == true
    float pos_femur_deg;
    float pos_tibia_deg;
    int64_t last_update_us; // esp_timer timestamp of the last successful response
} leg_telemetry_t;

// Stored-parameter IDs (see RS485_PROTOCOL.md "Stored Parameters").
typedef enum {
    RS485_PARAM_MOVE_DURATION    = 0x01, // uint16 ms
    RS485_PARAM_WATCHDOG_TIMEOUT = 0x02, // uint16 ms
    RS485_PARAM_INTERP_MODE      = 0x03, // uint8 (0=LINEAR, 1=CUBIC)
    RS485_PARAM_COXA_LIMIT_MIN   = 0x04, // int16, 0.1 deg
    RS485_PARAM_COXA_LIMIT_MAX   = 0x05,
    RS485_PARAM_FEMUR_LIMIT_MIN  = 0x06,
    RS485_PARAM_FEMUR_LIMIT_MAX  = 0x07,
    RS485_PARAM_TIBIA_LIMIT_MIN  = 0x08,
    RS485_PARAM_TIBIA_LIMIT_MAX  = 0x09,
} rs485_param_id_t;

// Initialize UART2 + transceiver and start the bus master task.
// Safe to call once during boot, before the motion loop starts.
esp_err_t rs485_master_init(void);

// Set the desired joint angles (degrees) for one leg. Non-blocking: copies into
// the shared command buffer consumed by the master task. leg index is 0-based.
void rs485_master_set_leg_angles(int leg, float coxa_deg, float femur_deg, float tibia_deg);

// Copy the latest telemetry snapshot for one leg into *out.
// Returns true if a snapshot has ever been received (out->stale indicates
// whether it is current). Returns false for an out-of-range leg or before any
// response has been seen.
bool rs485_master_get_telemetry(int leg, leg_telemetry_t *out);

// Queue a stored-parameter update for one leg. It is piggybacked on the next
// pull frame to that leg and re-sent automatically until acknowledged.
void rs485_master_queue_param(int leg, rs485_param_id_t param, int32_t value);

// Request that the next pull to this leg ask for joint positions in the
// response (JOINT_POS flag). One-shot: cleared after the next transaction.
void rs485_master_request_positions(int leg);

#endif // RS485_MASTER_H
