#include "interp.h"
#include <stdbool.h>

typedef struct {
    float pos;          // current interpolated position (deg)
    float vel;          // current velocity (deg/s) — for C1 restart continuity
    float start_pos;
    float start_vel;
    float target_pos;
    float target_vel;   // estimated velocity at the target (cubic only)
    float prev_target;  // previous commanded target (for velocity estimation)
    bool  have_prev;
    uint32_t move_start_us;
} joint_interp_t;

static joint_interp_t s_j[NUM_JOINTS];
static int      s_mode = DEFAULT_INTERP_MODE;
static uint32_t s_duration_us = DEFAULT_MOVE_DURATION_MS * 1000U;

void interp_init(float initial_deg)
{
    for (int i = 0; i < NUM_JOINTS; ++i) {
        s_j[i].pos = s_j[i].start_pos = s_j[i].target_pos = initial_deg;
        s_j[i].vel = s_j[i].start_vel = s_j[i].target_vel = 0.0f;
        s_j[i].prev_target = initial_deg;
        s_j[i].have_prev = false;
        s_j[i].move_start_us = 0;
    }
}

void interp_set_mode(int mode) { s_mode = mode; }

void interp_set_duration_ms(uint16_t ms)
{
    if (ms == 0) ms = 1;
    s_duration_us = (uint32_t)ms * 1000U;
}

void interp_set_target(int joint, float target_deg, uint32_t now_us)
{
    if (joint < 0 || joint >= NUM_JOINTS) return;
    joint_interp_t *j = &s_j[joint];

    // Restart the segment from the current state (C1 continuity).
    j->start_pos = j->pos;
    j->start_vel = j->vel;
    j->target_pos = target_deg;

    // Estimate the velocity to carry through the target from the command
    // stream, assuming commands arrive roughly every move duration.
    float dur_s = (float)s_duration_us / 1000000.0f;
    if (j->have_prev && dur_s > 0.0f) {
        j->target_vel = (target_deg - j->prev_target) / dur_s;
    } else {
        j->target_vel = 0.0f;
    }
    j->prev_target = target_deg;
    j->have_prev = true;
    j->move_start_us = now_us;
}

static void eval_joint(joint_interp_t *j, uint32_t now_us)
{
    float dur_s = (float)s_duration_us / 1000000.0f;
    if (dur_s <= 0.0f) { j->pos = j->target_pos; j->vel = 0.0f; return; }

    float s = (float)(now_us - j->move_start_us) / (float)s_duration_us;
    if (s < 0.0f) s = 0.0f;
    if (s > 1.0f) s = 1.0f;

    if (s_mode == INTERP_MODE_CUBIC) {
        float p0 = j->start_pos, p1 = j->target_pos;
        float m0 = j->start_vel * dur_s;   // tangents scaled by duration
        float m1 = j->target_vel * dur_s;
        float s2 = s * s, s3 = s2 * s;
        float h00 = 2*s3 - 3*s2 + 1;
        float h10 = s3 - 2*s2 + s;
        float h01 = -2*s3 + 3*s2;
        float h11 = s3 - s2;
        j->pos = h00*p0 + h10*m0 + h01*p1 + h11*m1;
        // Derivative wrt s, converted to per-second.
        float d00 = 6*s2 - 6*s;
        float d10 = 3*s2 - 4*s + 1;
        float d01 = -6*s2 + 6*s;
        float d11 = 3*s2 - 2*s;
        float dpds = d00*p0 + d10*m0 + d01*p1 + d11*m1;
        j->vel = dpds / dur_s;
    } else {
        // LINEAR
        j->pos = j->start_pos + (j->target_pos - j->start_pos) * s;
        j->vel = (s < 1.0f) ? (j->target_pos - j->start_pos) / dur_s : 0.0f;
    }
}

void interp_update(uint32_t now_us)
{
    for (int i = 0; i < NUM_JOINTS; ++i) {
        eval_joint(&s_j[i], now_us);
    }
}

float interp_get_pos(int joint)
{
    if (joint < 0 || joint >= NUM_JOINTS) return 0.0f;
    return s_j[joint].pos;
}

void interp_hold(uint32_t now_us)
{
    for (int i = 0; i < NUM_JOINTS; ++i) {
        s_j[i].start_pos = s_j[i].target_pos = s_j[i].pos;
        s_j[i].start_vel = s_j[i].target_vel = 0.0f;
        s_j[i].vel = 0.0f;
        s_j[i].move_start_us = now_us;
    }
}
