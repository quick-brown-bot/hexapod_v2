#include "swing_trajectory.h"
#include "gait_scheduler.h"
#include "motion_math.h"
#include <math.h>
#include <stdbool.h>
#include <assert.h>
#include "robot_config.h"

void swing_trajectory_init(swing_trajectory_t *trajectory, float step_length, float clearance_height) {
    trajectory->step_length = step_length;
    trajectory->clearance_height = clearance_height;
    trajectory->y_range_m = 0.05f; // +/-5 cm lateral
    trajectory->z_min_m = -0.05f;   // 5 cm min body height (avoid hitting battery)
    trajectory->z_max_m = -0.15f;   // 15 cm max body height
    trajectory->max_yaw_per_cycle_rad = 0.4f;
    trajectory->turn_direction = 1.0f;
    for (int i = 0; i < NUM_LEGS; ++i) {
        trajectory->desired_positions[i].x = 0.0f;
        trajectory->desired_positions[i].y = 0.0f;
        trajectory->desired_positions[i].z = 0.0f;
    }
}

void swing_trajectory_generate(swing_trajectory_t *trajectory, const gait_scheduler_t *scheduler, const user_command_t *cmd) {
    assert(trajectory != NULL);
    assert(scheduler != NULL);
    assert(cmd != NULL);
    // Normalize planar velocity command to the unit disc (X forward, Y left).
    // vx from left stick vertical, vy (strafe) from right stick horizontal.
    float vx_n = clampf(cmd->vx, -1.0f, 1.0f);
    float vy_n = clampf(cmd->vy, -1.0f, 1.0f);
    // Normalize wz to [-1,1] for yaw turning command
    float wz_n = clampf(cmd->wz, -1.0f, 1.0f);
    // Unit heading of the step + speed magnitude clamped to the unit disc, so a
    // diagonal command doesn't get sqrt(2)x the step length. Defaults to +X when
    // the command is ~zero.
    float raw_mag = sqrtf(vx_n * vx_n + vy_n * vy_n);
    float dir_x = 1.0f, dir_y = 0.0f;
    if (raw_mag > 1e-6f) {
        dir_x = vx_n / raw_mag;
        dir_y = vy_n / raw_mag;
    }
    float speed_mag = clampf(raw_mag, 0.0f, 1.0f);
    float scale = clampf(cmd->step_scale, 0.0f, 1.0f);
    bool enabled = cmd->enable;
    if (!enabled) {
        speed_mag = 0.0f;
        wz_n = 0.0f;  // disable turning when robot is disabled
    }
    float L = trajectory->step_length * scale * speed_mag; // effective step length (meters)
    
    float yaw_rotation = wz_n * trajectory->max_yaw_per_cycle_rad * scale * trajectory->turn_direction;
    
    float clr = trajectory->clearance_height * (cmd->terrain_climb ? 1.5f : 1.0f);
    // Map normalized pose to meters (Z UP): z_target in [-1,1] -> [z_min_m, z_max_m]
    // z_min_m: lower foot height (closer to ground), z_max_m: higher (lifted)
    float z_n = clampf(cmd->z_target, -1.0f, 1.0f);
    float body_z = trajectory->z_min_m + 0.5f * (z_n + 1.0f) * (trajectory->z_max_m - trajectory->z_min_m);
    float body_y = clampf(cmd->y_offset, -1.0f, 1.0f) * trajectory->y_range_m; // meters

    // Swing fraction S (0 < S < 1) and per-leg phase offsets come from the
    // gait scheduler's Wilson continuum (gait_scheduler_update()) - a single
    // formula parameterized by commanded speed, replacing the old hardcoded
    // per-gait offset tables here.
    float S = scheduler->duty_factor;
    assert(S > 0.0f && S < 1.0f);

    float phase = scheduler->phase;

    for (int i = 0; i < NUM_LEGS; ++i) {
        foot_position_t *p = &trajectory->desired_positions[i];
        float p_i = fracf(phase + scheduler->leg_phase_offsets[i]); // local phase of the individual leg

        // Only swing (lift foot) when there's a non-zero commanded step length or turn
        // This avoids up/down motion at zero speed.
        bool swing = enabled && ((L > 1e-6f) || (fabsf(yaw_rotation) > 1e-6f)) && (p_i < S);
        // tau progresses 0..1 within the active subphase (swing or support)
        float tau = swing ? (p_i / S) : ((p_i - S) / (1.0f - S));
        tau = clampf(tau, 0.0f, 1.0f);

        // Cycloid-like arc for swing; flat for support. Step displacement runs
        // along the planar heading (dir_x, dir_y), so pure vx walks forward,
        // pure vy strafes, and a mix walks diagonally.
        // Quintic (not linear) sweep for swing: zero horizontal velocity/
        // acceleration at lift-off (tau=0) and touchdown (tau=1) avoids the
        // instantaneous velocity reversal a linear sweep has at the
        // swing->support boundary, which was causing foot slip and servo
        // shock. Also used to shape the yaw-turn sweep below for the same
        // reason.
        float tau_q = swing ? quintic_smoothstep(tau) : tau;

        float disp; // signed offset along heading, -L/2 .. +L/2
        float z_rel; // Z up: 0 at stance, positive during lift
        if (swing) {
            disp = (-0.5f + tau_q) * L;
            z_rel =  clr * sinf((float)M_PI * tau); // peak at mid
        } else {
            // support: travel backward along heading at ground height
            disp = (0.5f - tau) * L;
            z_rel = 0.0f;
        }
        float x_rel = disp * dir_x;
        float y_rel = disp * dir_y;

        // Convert offsets to absolute body-frame targets by adding each leg's base pose
        float bx, by, bz, yaw;
        robot_config_get_base_pose(i, &bx, &by, &bz, &yaw);
        
        // Calculate yaw rotation effect on stance position
        // During turning, each leg moves in an arc around the body center
        float yaw_offset = 0.0f;
        if (fabsf(yaw_rotation) > 1e-6f) {
            // Calculate the angular displacement for this leg during this phase
            // Support legs move backward relative to body rotation, swing legs follow the rotation
            if (swing) {
                // During swing, foot follows the body rotation
                yaw_offset = yaw_rotation * tau_q;
            } else {
                // During support, foot moves backward relative to body rotation
                yaw_offset = -yaw_rotation * tau;
            }
        }
        
        // Apply neutral stance reach in leg-local frame (X_outward, Y_forward), rotated to body frame
        // with yaw rotation applied
        float out_m = robot_config_get_stance_out_m(i); 
        float fwd_m = robot_config_get_stance_fwd_m(i);
        float total_yaw = yaw + yaw_offset;
        float cy = cosf(total_yaw), sy = sinf(total_yaw);
        float dx_stance = cy * out_m - sy * fwd_m;
        float dy_stance = sy * out_m + cy * fwd_m;

        p->x = bx + dx_stance + x_rel;
        p->y = by + dy_stance + y_rel + body_y;
        p->z = bz + body_z + z_rel; // Z up absolute
        
        // Debug logging for leg 0 to monitor turning behavior
        // if (i == 0) {
        //     ESP_LOGI(TAG, "Leg %d: bXYZ=(%.3f, %.3f, %.3f) yaw=%.3f yaw_off=%.3f total_yaw=%.3f dXYZ=(%.3f, %.3f, %.3f) p_i=%.3f swing=%d tau=%.3f x_rel=%.3f body_y=%.3f z_rel=%.3f -> pos=(%.3f, %.3f, %.3f)", 
        //              i, bx, by, bz, yaw, yaw_offset, total_yaw, dx_stance, dy_stance, body_z, p_i, swing, tau, x_rel, body_y, z_rel, p->x, p->y, p->z);
        // }
        // TODO: Add simple body roll/pitch offsets when pose_mode is active
    }
}
