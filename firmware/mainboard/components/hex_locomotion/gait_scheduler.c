#include "gait_scheduler.h"
#include "robot_config.h"
#include "motion_math.h"
#include <assert.h>
#include <math.h>

// Wilson gait continuum: a canonical stepping order used to derive per-leg
// phase offsets from a single duty-factor parameter, instead of a hardcoded
// offset pattern per discrete gait. The order traces a continuous loop
// around the body (down the left side front->rear, across, up the right
// side rear->front): LF, LM, LR, RR, RM, RF. Consecutive legs in this order
// are spaced by exactly `duty_factor` (S) in phase.
//
// This single rule reproduces both endpoints of the old discrete-gait table
// exactly:
//  - S = 1/NUM_LEGS (~0.1, old wave): offsets are 0, 1/6, 2/6, 3/6, 4/6, 5/6
//    around the loop -> one leg swinging at a time, sequential wave.
//  - S = 0.5 (old tripod): offsets collapse to only two values (0 and 0.5,
//    since 2*0.5 = 1 mod 1) landing on exactly the classic tripod groups
//    {LF, LR, RM} vs {LM, RR, RF} -> 3 legs swinging at once.
// In between, it interpolates continuously (a ripple-like gait with 2 legs
// swinging at once falls naturally out of the same formula at S = 1/3).
//
// Values are indexed by leg_index (see robot_config.h: LEG_LEFT_FRONT=0,
// LEG_LEFT_MIDDLE=1, LEG_LEFT_REAR=2, LEG_RIGHT_FRONT=3, LEG_RIGHT_MIDDLE=4,
// LEG_RIGHT_REAR=5) and give each leg's position in the canonical order
// above.
static const float kLegOrderPosition[NUM_LEGS] = {
    0.0f, // LEG_LEFT_FRONT
    1.0f, // LEG_LEFT_MIDDLE
    2.0f, // LEG_LEFT_REAR
    5.0f, // LEG_RIGHT_FRONT
    4.0f, // LEG_RIGHT_MIDDLE
    3.0f, // LEG_RIGHT_REAR
};

void gait_scheduler_init(gait_scheduler_t *scheduler, float cycle_time) {
    // Initialize scheduler parameters
    scheduler->cycle_time = cycle_time;
    scheduler->phase = 0.0f;
    scheduler->duty_factor_min = 0.1f;
    scheduler->duty_factor_max = 0.5f;
    scheduler->duty_factor = scheduler->duty_factor_min;
    for (int i = 0; i < NUM_LEGS; ++i) {
        scheduler->leg_states[i] = LEG_SUPPORT;
        scheduler->leg_phase_offsets[i] = kLegOrderPosition[i] * scheduler->duty_factor_min;
    }
}

void gait_scheduler_update(gait_scheduler_t *scheduler, float dt, const user_command_t *cmd) {
    assert(scheduler != NULL);
    assert(cmd != NULL);
    // Advance phase only when enabled and commanded velocity is non-zero
    // TODO: Consider separate forward/turning components and modulate phase rate by
    //       a base frequency parameter instead of reusing cycle_time directly.
    float planar_speed = sqrtf(cmd->vx * cmd->vx + cmd->vy * cmd->vy);
    float speed_mag = clampf(planar_speed, 0.0f, 1.0f);
    if (cmd->enable && (planar_speed > 1e-3f || fabsf(cmd->wz) > 1e-3f)) {
        // crude frequency scaling: base 1/cycle_time Hz, scaled by step_scale and planar speed
        float speed = planar_speed; // combined forward+strafe magnitude
        float freq = (scheduler->cycle_time > 0.0f) ? (1.0f / scheduler->cycle_time) : 1.0f;
        float scale = (cmd->step_scale > 0.0f) ? cmd->step_scale : 0.5f;
        float phase_rate = freq * (0.5f + 0.5f * (speed > 1.0f ? 1.0f : speed)) * scale; // 0.5..1x based on speed
        scheduler->phase += phase_rate * dt;
        if (scheduler->phase >= 1.0f) {
            scheduler->phase -= 1.0f;
        }
    } else {
        // hold phase and keep all legs in support when disabled or zero speed
        for (int i = 0; i < NUM_LEGS; ++i) {
            scheduler->leg_states[i] = LEG_SUPPORT;
        }
        scheduler->phase = 0.0f;
        return;
    }

    // Wilson gait continuum: map commanded speed to a duty factor (swing
    // fraction) between the configured bounds. The operator's gait switch
    // (`cmd->gait`) no longer selects a hardcoded offset table; instead it
    // biases which part of the continuum is reachable, so the 3 physical
    // switch positions keep a useful, distinct meaning:
    //   - GAIT_WAVE:   cap the continuum to its cautious end (max-stability
    //                  stepping) regardless of stick position.
    //   - GAIT_TRIPOD: pin the continuum to its fast end (matches the old
    //                  fixed tripod gait exactly, for max agility/benchmarking).
    //   - GAIT_RIPPLE: full continuum, auto-selected purely from speed - the
    //                  "normal" driving mode.
    float speed_eff;
    switch (cmd->gait) {
        case GAIT_WAVE:   speed_eff = clampf(speed_mag, 0.0f, 0.35f); break;
        case GAIT_TRIPOD: speed_eff = 1.0f; break;
        case GAIT_RIPPLE: default: speed_eff = speed_mag; break;
    }
    float t = quintic_smoothstep(speed_eff); // smooth in/out, no pop at the ends
    float duty_factor = scheduler->duty_factor_min +
                         (scheduler->duty_factor_max - scheduler->duty_factor_min) * t;
    scheduler->duty_factor = duty_factor;

    for (int i = 0; i < NUM_LEGS; ++i) {
        float offset = fracf(kLegOrderPosition[i] * duty_factor);
        scheduler->leg_phase_offsets[i] = offset;
        float p_i = fracf(scheduler->phase + offset);
        scheduler->leg_states[i] = (p_i < duty_factor) ? LEG_SWING : LEG_SUPPORT;
    }
}
