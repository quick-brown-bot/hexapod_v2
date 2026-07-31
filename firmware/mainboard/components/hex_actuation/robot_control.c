#include "robot_control.h"
#include "esp_log.h"
#include "rs485_master.h"
#include "robot_config.h"
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const char *TAG_RC = "robot_control";

// V2 actuation: the ESP32 no longer generates servo PWM. Each RP2040 LegBoard
// drives its own servos locally. This layer forwards desired joint angles to
// hex_rs485_master, which delivers them to the legs over RS485. Calibration,
// joint offsets, sign inversion, and PWM mapping all live on the RP2040 — this
// layer performs only the radians -> degrees unit conversion for the wire
// protocol. See docs/architecture/SYSTEM_ARCHITECTURE.md.

static inline float rad_to_deg(float rad) {
    return rad * (float)(180.0 / M_PI);
}

// Forward a single joint angle (radians) to the RS485 command buffer.
// Retained for API compatibility with V1; V2 sends a whole leg at once, so
// this buffers per-joint by reading back is not possible — callers should
// prefer robot_execute(). Provided as a thin per-joint helper.
esp_err_t robot_set_joint_angle_rad(int leg_index, leg_servo_t joint, float radians) {
    if (leg_index < 0 || leg_index >= NUM_LEGS) {
        return ESP_ERR_INVALID_ARG;
    }
    // A per-joint setter cannot assemble a full 3-angle leg command on its own.
    // Log at verbose level; whole-body path (robot_execute) is the supported one.
    ESP_LOGV(TAG_RC, "robot_set_joint_angle_rad leg %d joint %d -> %.2f deg (use robot_execute for V2)",
             leg_index, (int)joint, rad_to_deg(radians));
    return ESP_OK;
}

void robot_execute(const whole_body_cmd_t *cmds) {
    if (!cmds) return;
    // whole_body_cmd_t carries IK joint angles in radians per leg (coxa, femur, tibia).
    for (int i = 0; i < NUM_LEGS; ++i) {
        const float coxa  = cmds->joint_cmds[i].joint_angles[0];
        const float femur = cmds->joint_cmds[i].joint_angles[1];
        const float tibia = cmds->joint_cmds[i].joint_angles[2];
        rs485_master_set_leg_angles(i, rad_to_deg(coxa), rad_to_deg(femur), rad_to_deg(tibia));
    }
}
