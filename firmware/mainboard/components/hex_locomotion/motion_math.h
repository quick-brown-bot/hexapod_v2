#ifndef HEX_LOCOMOTION_MOTION_MATH_H
#define HEX_LOCOMOTION_MOTION_MATH_H

#include <math.h>

// Small math helpers shared by gait_scheduler.c and swing_trajectory.c.
// Header-only (static inline) since these are one-liners used in a hot,
// per-tick path; not worth a separate translation unit.

static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Wraps x into [0, 1).
static inline float fracf(float x) {
    float f = x - floorf(x);
    return (f < 0.0f) ? (f + 1.0f) : f;
}

// Minimum-jerk (quintic) smoothstep: maps t in [0,1] to [0,1] with zero
// velocity AND zero acceleration at both endpoints.
static inline float quintic_smoothstep(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

#endif // HEX_LOCOMOTION_MOTION_MATH_H
