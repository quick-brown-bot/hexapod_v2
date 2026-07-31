# hex_locomotion

## Role
Locomotion command and trajectory pipeline.

## Contents
- user_command.c
- gait_scheduler.c/.h
- swing_trajectory.c/.h
- whole_body_control.c/.h

## Responsibilities
- Poll and map controller input into user commands.
- Compute gait phase and leg support/swing states.
- Generate swing trajectories.
- Convert trajectories into per-leg joint commands.
