# hex_actuation

## Role
Final stage of the motion pipeline: converts whole-body/IK joint commands into
RS485 leg commands. The ESP32 does **not** generate servo PWM itself in V2 —
that happens locally on each LegBoard (RP2040) — so this component's job is
forwarding, not driving hardware directly. See
`docs/architecture/SYSTEM_ARCHITECTURE.md` and
`docs/architecture/HARDWARE_AND_MECHANICS.md`.

## Contents
- robot_control.c
- robot_control.h

## Responsibilities
- `robot_execute()`: called every 10ms from the gait loop (`main.c`) with the
  whole-body command; converts each leg's IK joint angles (radians) to degrees
  and forwards them to `hex_rs485_master` via `rs485_master_set_leg_angles()`.
  Performs the unit conversion only — no clamping, offsets, or PWM mapping,
  which all live on the LegBoard (see `firmware/leg/src/servo.cpp`).
- Manual per-leg joint override for RS485 bring-up/diagnostics: while active
  for a leg, `robot_execute()` sends the overridden angles for that leg
  instead of the whole-body IK output, so a single joint (or an IK-computed
  Cartesian target) can be exercised directly without the gait loop
  overwriting it 10ms later. Driven by `hex_rpc_core`'s `joint`/`ik` RPC
  commands; cleared per-leg automatically when released.

## Public Surface
- Header: robot_control.h
- Core APIs:
  - `robot_execute(const whole_body_cmd_t *cmds)`
  - `robot_set_leg_joint_override_deg(int leg_index, float coxa_deg, float femur_deg, float tibia_deg)`
  - `robot_clear_leg_joint_override(int leg_index)`
  - `robot_set_joint_angle_rad(...)` — retained for API compatibility with a
    pre-V2 per-joint path; logs only, not the supported way to command a
    joint (use `robot_execute()`, or the manual override above for
    diagnostics).

## Integration
- Consumes `whole_body_cmd_t` from `hex_locomotion`'s whole-body control output.
- Writes to `hex_rs485_master`'s per-leg command buffer.
- Uses `hex_robot_config`/`hex_kinematics` types (`NUM_LEGS`, `leg_servo_t`).
- The manual override is read from and written by two different tasks (the
  100Hz gait loop task and the RPC processing task), so it's guarded by a
  `portMUX_TYPE` spinlock rather than a heap-allocated mutex, avoiding any
  first-use initialization race between them.

## SDKConfig Requirements (Current Project)
- No dedicated module-specific Kconfig toggles are required.
- Depends on project FreeRTOS support (the override spinlock) and the RS485
  UART configuration owned by `hex_rs485_master`.
