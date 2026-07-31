# hex_actuation

## Role
Servo actuation backend and low-level joint output path.

## Contents
- robot_control.c
- robot_control.h

## Responsibilities
- Convert joint commands into servo pulses.
- Apply calibration and output mapping.
- Drive MCPWM and LEDC backends.
