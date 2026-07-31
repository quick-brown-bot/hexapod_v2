// RS485 frame protocol — parsing and building.
//
// This module is intentionally free of any Arduino / Pico SDK dependency so it
// can be unit-tested on the host (see test/). It must stay byte-for-byte
// compatible with the ESP32 master in
//   firmware/mainboard/components/hex_rs485_master/rs485_master.c
// and the wire format in docs/interfaces/RS485_PROTOCOL.md.

#ifndef HEX_LEG_PROTOCOL_H
#define HEX_LEG_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROTO_MAX_PARAMS 8

// Pull flags (master -> leg)
#define PROTO_FLAG_JOINT_POS 0x01

// Response status flags (leg -> master)
#define PROTO_STATUS_CONFIG_APPLIED  0x01
#define PROTO_STATUS_WATCHDOG_ACTIVE 0x02
#define PROTO_STATUS_LIMIT_CLAMP     0x04

// Stored-parameter IDs (must match the master's rs485_param_id_t and
// RS485_PROTOCOL.md "Stored Parameters").
#define RS485_PARAM_MOVE_DURATION_ID     0x01
#define RS485_PARAM_WATCHDOG_TIMEOUT_ID  0x02
#define RS485_PARAM_INTERP_MODE_ID       0x03
#define RS485_PARAM_COXA_MIN_ID          0x04
#define RS485_PARAM_COXA_MAX_ID          0x05
#define RS485_PARAM_FEMUR_MIN_ID         0x06
#define RS485_PARAM_FEMUR_MAX_ID         0x07
#define RS485_PARAM_TIBIA_MIN_ID         0x08
#define RS485_PARAM_TIBIA_MAX_ID         0x09

// CRC-8/SMBus: polynomial 0x07, init 0x00, no reflection, no final XOR.
// Identical to the master's crc8(). Covers all bytes from the leading
// '>'/'<' up to (not including) '*'.
uint8_t proto_crc8(const uint8_t *data, size_t len);

// One config entry parsed from a pull frame.
typedef struct {
    uint8_t id;      // parameter id (hex in the frame, e.g. P01 -> 0x01)
    int32_t value;   // decimal value
} proto_param_t;

// Result of parsing a pull frame.
typedef struct {
    uint8_t addr;          // 1..6
    uint8_t flags;         // PROTO_FLAG_*
    float   coxa_deg;
    float   femur_deg;
    float   tibia_deg;
    int     n_params;
    proto_param_t params[PROTO_MAX_PARAMS];
} proto_pull_t;

// Parse a pull frame. `line` is NUL-terminated and is the frame content with
// an optional trailing '\n' already stripped or present (either is tolerated).
// Validates the leading '>' and the CRC. Returns true on success.
// NOTE: this mutates `line` in place (tokenization). Pass a writable buffer.
bool proto_parse_pull(char *line, proto_pull_t *out);

// Fields to format into a telemetry response.
typedef struct {
    uint8_t  addr;
    uint8_t  status;
    uint16_t current_total_ma;
    uint16_t current_coxa_ma;
    uint16_t current_femur_ma;
    uint16_t current_tibia_ma;
    bool     include_positions;   // emit joint positions if true
    float    pos_coxa_deg;
    float    pos_femur_deg;
    float    pos_tibia_deg;
} proto_response_t;

// Build a telemetry response into `buf` (including the trailing '\n').
// Returns the number of bytes written (excluding the NUL terminator), or -1 on
// overflow.
int proto_build_response(const proto_response_t *r, char *buf, size_t buf_sz);

#ifdef __cplusplus
}
#endif

#endif // HEX_LEG_PROTOCOL_H
