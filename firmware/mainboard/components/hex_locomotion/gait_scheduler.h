#ifndef GAIT_SCHEDULER_H
#define GAIT_SCHEDULER_H

#include <stdint.h>
#include "user_command.h"

#define NUM_LEGS 6

typedef enum {
    LEG_SUPPORT,
    LEG_SWING
} leg_state_t;

typedef struct {
    leg_state_t leg_states[NUM_LEGS];
    float cycle_time;
    float phase;
    // Wilson gait continuum bounds (swing-fraction/duty-factor), copied from
    // gait_config_t at init; see config_ns_gait_api.h for the derivation.
    float duty_factor_min;
    float duty_factor_max;
    // Outputs of the continuum, recomputed every gait_scheduler_update():
    float duty_factor;                 // S: current swing fraction, 0 < S < 1
    float leg_phase_offsets[NUM_LEGS]; // per-leg phase offset in [0,1)
} gait_scheduler_t;

void gait_scheduler_init(gait_scheduler_t *scheduler, float cycle_time);
// Update leg phases/states based on command and timestep. Also recomputes
// duty_factor and leg_phase_offsets (Wilson gait continuum): the swing
// fraction and per-leg phase spacing scale continuously with commanded
// speed instead of switching between 3 discrete gaits.
void gait_scheduler_update(gait_scheduler_t *scheduler, float dt, const user_command_t *cmd);

#endif // GAIT_SCHEDULER_H
