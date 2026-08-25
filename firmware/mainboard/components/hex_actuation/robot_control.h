#ifndef ROBOT_CONTROL_H
#define ROBOT_CONTROL_H

#include "whole_body_control.h"
#include "leg.h" // for leg_servo_t enum
#include "esp_err.h"

// Apply whole-body command: consumes leg-local Cartesian targets, runs IK, and drives servos.
void robot_execute(const whole_body_cmd_t *cmds);

// Low-level joint command API (host for moved functionality from leg.c).
// Implementations will clamp, apply offsets, and command servos per joint.
esp_err_t robot_set_joint_angle_rad(int leg_index, leg_servo_t joint, float radians);

// Manual per-leg joint override for RS485 bring-up/diagnostics (e.g. the RPC "joint"
// command). While active for a leg, robot_execute() sends these angles (degrees) for
// that leg instead of the whole-body IK output, so a single joint can be exercised
// directly without the gait loop overwriting it 10ms later. Cleared per-leg with
// robot_clear_leg_joint_override().
void robot_set_leg_joint_override_deg(int leg_index, float coxa_deg, float femur_deg, float tibia_deg);
void robot_clear_leg_joint_override(int leg_index);

#endif // ROBOT_CONTROL_H
