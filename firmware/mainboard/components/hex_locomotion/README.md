# hex_locomotion

## Role
Locomotion command and trajectory pipeline.

## Contents
- user_command.c
- gait_scheduler.c/.h
- swing_trajectory.c/.h
- whole_body_control.c/.h
- motion_math.h (shared clampf/fracf/quintic_smoothstep helpers)

## Responsibilities
- Poll and map controller input into user commands.
- Compute gait phase and leg support/swing states, using a Wilson gait
  continuum: swing fraction (duty factor) and per-leg phase offsets scale
  continuously with commanded speed instead of switching between 3
  hardcoded gaits (see `gait_scheduler.c` and the `gait` namespace's
  `duty_factor_min`/`duty_factor_max`).
- Generate swing trajectories, with a quintic (minimum-jerk) horizontal
  sweep during swing for zero-velocity touchdown.
- Convert trajectories into per-leg joint commands.
