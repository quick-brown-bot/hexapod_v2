// Hexapod V2 LegBoard firmware (RP2040 / arduino-pico).
//
// Role: RS485 slave that drives three servos for one leg. Receives sparse
// joint-angle targets from the ESP32 master, interpolates locally for smooth
// motion, samples per-servo current, and answers every pull with a telemetry
// response. Holds position if the bus goes silent (watchdog).
//
// See:
//   docs/architecture/HARDWARE_AND_MECHANICS.md
//   docs/interfaces/RS485_PROTOCOL.md
//   docs/architecture/SYSTEM_ARCHITECTURE.md

#include <Arduino.h>

#include "config.h"
#include "protocol.h"
#include "rs485.h"
#include "servo.h"
#include "current.h"
#include "interp.h"
#include "persist.h"
#include "calib.h"
#include "status_led.h"

// --- Runtime state -------------------------------------------------------
static uint8_t  s_addr = DEFAULT_LEG_ADDR;
static uint16_t s_watchdog_timeout_ms = DEFAULT_WATCHDOG_TIMEOUT_MS;
static bool     s_watchdog_active = false;

// Joint hard limits (degrees). Updated via params P04..P09.
static float s_jmin[NUM_JOINTS] = { DEFAULT_JOINT_MIN_DEG, DEFAULT_JOINT_MIN_DEG, DEFAULT_JOINT_MIN_DEG };
static float s_jmax[NUM_JOINTS] = { DEFAULT_JOINT_MAX_DEG, DEFAULT_JOINT_MAX_DEG, DEFAULT_JOINT_MAX_DEG };

static uint32_t s_last_rx_us = 0;
static uint32_t s_last_control_us = 0;

static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Apply one config parameter carried in a pull frame.
static void apply_param(const proto_param_t *p)
{
    switch (p->id) {
        case RS485_PARAM_MOVE_DURATION_ID:    interp_set_duration_ms((uint16_t)p->value); break;
        case RS485_PARAM_WATCHDOG_TIMEOUT_ID: s_watchdog_timeout_ms = (uint16_t)p->value; break;
        case RS485_PARAM_INTERP_MODE_ID:      interp_set_mode((int)p->value); break;
        case RS485_PARAM_COXA_MIN_ID:  s_jmin[JOINT_COXA]  = p->value / 10.0f; break;
        case RS485_PARAM_COXA_MAX_ID:  s_jmax[JOINT_COXA]  = p->value / 10.0f; break;
        case RS485_PARAM_FEMUR_MIN_ID: s_jmin[JOINT_FEMUR] = p->value / 10.0f; break;
        case RS485_PARAM_FEMUR_MAX_ID: s_jmax[JOINT_FEMUR] = p->value / 10.0f; break;
        case RS485_PARAM_TIBIA_MIN_ID: s_jmin[JOINT_TIBIA] = p->value / 10.0f; break;
        case RS485_PARAM_TIBIA_MAX_ID: s_jmax[JOINT_TIBIA] = p->value / 10.0f; break;
        default: break; // unknown id: ignore
    }
}

// Handle one validated pull frame addressed to this leg: apply targets/config,
// then send the telemetry response (the response implicitly ACKs config).
static void handle_pull(const proto_pull_t *pull, uint32_t now_us)
{
    uint8_t status = 0;

    if (pull->n_params > 0) {
        for (int i = 0; i < pull->n_params; ++i) apply_param(&pull->params[i]);
        status |= PROTO_STATUS_CONFIG_APPLIED;
    }

    const float raw[NUM_JOINTS] = { pull->coxa_deg, pull->femur_deg, pull->tibia_deg };
    for (int j = 0; j < NUM_JOINTS; ++j) {
        float limited = clampf(raw[j], s_jmin[j], s_jmax[j]);
        if (limited != raw[j]) status |= PROTO_STATUS_LIMIT_CLAMP;
        interp_set_target(j, limited, now_us);
        servo_clear_override(j); // a real pull frame retakes control from any calib PWM override
    }

    // If we were in watchdog hold, report it once on this recovery response so
    // the master learns the leg held position during the silence. While
    // watchdog is active the leg sends nothing, so this is the only chance to
    // surface the bit.
    if (s_watchdog_active) status |= PROTO_STATUS_WATCHDOG_ACTIVE;
    s_last_rx_us = now_us;
    s_watchdog_active = false;

    current_reading_t cur;
    current_get(&cur);

    proto_response_t resp = {0};
    resp.addr = s_addr;
    resp.status = status;
    resp.current_total_ma = cur.total_ma;
    resp.current_coxa_ma  = cur.coxa_ma;
    resp.current_femur_ma = cur.femur_ma;
    resp.current_tibia_ma = cur.tibia_ma;

    if (pull->flags & PROTO_FLAG_JOINT_POS) {
        resp.include_positions = true;
        resp.pos_coxa_deg  = interp_get_pos(JOINT_COXA);
        resp.pos_femur_deg = interp_get_pos(JOINT_FEMUR);
        resp.pos_tibia_deg = interp_get_pos(JOINT_TIBIA);
    }

    char out[160];
    int n = proto_build_response(&resp, out, sizeof(out));
    if (n > 0) rs485_send(out, (size_t)n);
}

void setup()
{
    calib_init();        // USB serial (independent of RS485)
    persist_init();
    s_addr = persist_get_address();
    status_led_init();

    rs485_init();
    servo_init();
    current_init();
    interp_init(0.0f);   // start at neutral (0 deg)

    uint32_t now = micros();
    s_last_rx_us = now;
    s_last_control_us = now;
}

void loop()
{
    // Re-read every loop (cheap RAM read, not a flash access) so a fresh
    // ADDR over USB takes effect immediately -- including turning the status
    // LED off -- without requiring a reboot.
    s_addr = persist_get_address();
    status_led_update(s_addr != 0, micros());

    // 1) USB calibration commands (non-blocking).
    calib_poll();

    // 2) RS485: respond immediately to a valid pull for this leg. Frames that
    //    fail CRC or address-match produce no response (the master treats
    //    silence as a timeout — never NAK). addr 0 (uncalibrated) never
    //    matches -- the protocol only ever addresses legs 1-6.
    char *line = rs485_poll_line();
    if (line) {
        proto_pull_t pull;
        if (proto_parse_pull(line, &pull) && pull.addr == s_addr) {
            handle_pull(&pull, micros());
        }
    }

    // 3) Fixed-rate control step: interpolate, drive servos, sample current,
    //    enforce watchdog.
    uint32_t now = micros();
    if ((uint32_t)(now - s_last_control_us) >= CONTROL_PERIOD_US) {
        s_last_control_us = now;

        if ((uint32_t)(now - s_last_rx_us) > (uint32_t)s_watchdog_timeout_ms * 1000U) {
            if (!s_watchdog_active) {
                s_watchdog_active = true;
                interp_hold(now); // freeze at last commanded position
            }
        }

        interp_update(now);
        for (int j = 0; j < NUM_JOINTS; ++j) {
            servo_write_angle(j, interp_get_pos(j));
        }
        current_sample();
    }
}
