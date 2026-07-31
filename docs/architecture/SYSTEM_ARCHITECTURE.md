# Hexapod V2 System Architecture

## Purpose

This document is the firmware architecture reference for Version 2.

It describes:
- which firmware components make up the V2 system,
- what changed from V1 and why,
- how data moves through the runtime,
- how the RS485 communication task decouples leg communication from the motion loop.

For hardware topology, power distribution, and board responsibilities see
[HARDWARE_AND_MECHANICS.md](HARDWARE_AND_MECHANICS.md).

---

## What Differs From V1

V2 firmware is structurally identical to V1 with one significant addition: the
motion stack no longer drives servo hardware directly. Instead `hex_actuation`
writes desired joint angles to a shared command buffer, and a new
`hex_rs485_master` component owns a dedicated FreeRTOS task that sends those
angles to the six RP2040 leg controllers and immediately polls telemetry back
from each leg — all without blocking the 100 Hz locomotion loop.

Everything else — controller and transport stack, RPC and configuration stack,
gait scheduling, kinematics, motion limiting — is unchanged from V1.

---

## 1. System At A Glance

```mermaid
flowchart LR
    App[main app bootstrap and loop]

    subgraph Motion[Motion Stack]
        Loc[hex_locomotion]
        Lim[hex_motion_limits]
        Act[hex_actuation]
        Kin[hex_kinematics]
        RobotCfg[hex_robot_config]
    end

    subgraph RS485[RS485 Communication]
        RS485M[hex_rs485_master]
        CmdBuf[[Command buffer\n6x joint angles]]
        TelBuf[[Telemetry buffer\n6x current + status]]
    end

    subgraph Control[Controller and Transport]
        Core[hex_controller_core]
        Fly[hex_controller_driver_flysky_ibus]
        WifiDrv[hex_controller_driver_wifi_tcp]
        BtDrv[hex_controller_driver_bt_classic]
        WifiAp[hex_wifi_ap]
    end

    subgraph RPC[RPC and Config]
        RpcCore[hex_rpc_core]
        RpcTx[hex_rpc_transport]
        CfgMgr[hex_config_manager]
        Shared[hex_shared_types]
    end

    App --> Loc --> Lim --> Act
    Loc --> Kin
    Loc --> RobotCfg
    Lim --> Kin
    Lim --> RobotCfg
    Act --> RobotCfg
    Act --> CmdBuf
    CmdBuf --> RS485M
    RS485M --> TelBuf
    TelBuf --> Loc

    Fly --> Core
    WifiDrv --> Core
    BtDrv --> Core
    WifiDrv --> RpcTx
    BtDrv --> RpcTx
    WifiAp --> WifiDrv

    RpcCore --> RpcTx
    RpcCore --> CfgMgr
    RpcCore --> Core

    Shared --> Core
    Shared --> RpcCore
    Shared --> RobotCfg
```

---

## 2. Architectural Layers

### 2.1 Application Bootstrap

Owned by `main/`. Identical to V1 with one additional responsibility: initialize
`hex_rs485_master` and start the RS485 communication task before entering the
locomotion loop.

### 2.2 Motion Stack

The motion stack is identical to V1 through `kpp_apply_limits`. The only change
is in `hex_actuation`.

Components:
- `hex_locomotion`: user command mapping, gait scheduling, swing target
  generation, whole-body control. Unchanged from V1.
- `hex_motion_limits`: KPP-based velocity, acceleration, and jerk limiting plus
  state estimation. Unchanged from V1.
- `hex_actuation`: converts limited joint commands to wire format and writes
  them to the shared command buffer. Returns immediately — no bus I/O, no
  blocking. Same public API as V1; V2 implementation swaps PWM register writes
  for command buffer writes.
- `hex_kinematics`: math-only leg kinematics primitives. Unchanged from V1.
- `hex_robot_config`: geometry, mount pose, joint calibration, and servo mapping
  consumption. Unchanged from V1.

Design intent:
- No component in the motion stack touches the RS485 bus or blocks on leg
  communication.
- `hex_actuation` keeps the same public API as V1 so the rest of the motion
  stack requires no changes.

### 2.3 RS485 Communication Stack

New in V2. Owns the physical RS485 bus and all leg communication.

Components:
- `hex_rs485_master`: dedicated FreeRTOS task; owns the RS485 bus exclusively;
  executes a per-leg send-and-poll cycle; writes results to the telemetry
  buffer.
- **Command buffer**: shared memory written by `hex_actuation` (main loop) and
  read by `hex_rs485_master` (comm task). Holds desired joint angles and arrival
  times for all six legs. The comm task always reads the latest written value;
  writes from the main loop are non-blocking.
- **Telemetry buffer**: shared memory written by `hex_rs485_master` (comm task)
  and read by motion stack consumers. Holds the latest available per-leg
  telemetry (per-servo current, leg status). May be up to one comm cycle stale.

Per-leg transaction (executed by `hex_rs485_master` for each leg in sequence):

```
1. Read desired angles for leg N from command buffer.
2. Assert DE high — transmit mode.
3. Send command frame to leg N (desired joint angles + arrival time).
4. Release DE low — receive mode.
5. Wait for telemetry response from leg N (per-servo current + leg status).
6. Write telemetry to buffer entry for leg N.
```

This repeats for legs 1 through 6, then immediately restarts. The telemetry
buffer entry for each leg is updated as soon as its transaction completes — the
main loop does not need to wait for all six legs before any result is available.

Design intent:
- The RS485 bus is owned exclusively by `hex_rs485_master`. No other component
  asserts DE or writes to the bus.
- Send and poll are a single transaction per leg so current measurements are as
  fresh as the bus cycle allows. This is the minimum possible latency for
  detecting leg touchdown from a current spike.
- A single leg timing out must not stall the comm cycle. `hex_rs485_master`
  applies a per-leg response timeout and moves on to leg N+1.
- The comm task rate is determined by per-leg transaction time (baud rate, frame
  sizes, DE turnaround, leg response latency) and is independent of the 100 Hz
  main loop rate.

### 2.4 Controller And Transport Stack

Identical to V1. See
[../interfaces/CONTROLLER_DRIVERS.md](../interfaces/CONTROLLER_DRIVERS.md).

### 2.5 RPC And Configuration Stack

Identical to V1. See
[../interfaces/RPC_SYSTEM_DESIGN.md](../interfaces/RPC_SYSTEM_DESIGN.md)
and
[../configuration/CONFIGURATION_PERSISTENCE_DESIGN.md](../configuration/CONFIGURATION_PERSISTENCE_DESIGN.md).

---

## 3. Runtime Flows

### 3.1 Boot Flow

1. Initialize configuration manager and make namespace state available.
2. Start RPC transport/core services.
3. Bring up Wi-Fi AP and transport-facing controller services as configured.
4. Initialize robot runtime modules that consume loaded configuration.
5. Initialize `hex_rs485_master` and start the RS485 communication task.
6. Enter the 100 Hz locomotion loop.

RS485 initialization must complete before the locomotion loop starts so that the
comm task is running and the command buffer is being serviced from the first
loop cycle.

### 3.2 100 Hz Locomotion Loop

The pipeline is structurally identical to V1:

```mermaid
flowchart TD
    Poll[user_command_poll]
    Sched[gait_scheduler_update]
    Swing[swing_trajectory_generate]
    Control[whole_body_control_compute]
    Limit[kpp_apply_limits]
    Execute[robot_execute]
    State[kpp_update_state]

    Poll --> Sched --> Swing --> Control --> Limit --> Execute --> State
```

`robot_execute` (inside `hex_actuation`) writes desired joint angles and arrival
times to the command buffer and returns. The call never blocks on bus I/O.

Telemetry from the latest completed comm cycle is available in the telemetry
buffer at any point in the loop. Locomotion stages that need ground contact
information (e.g. current-spike-based touchdown detection) read from this buffer
non-blocking.

### 3.3 RS485 Communication Task

`hex_rs485_master` runs as an independent FreeRTOS task:

```mermaid
flowchart TD
    Start[Begin leg cycle: N = 1]
    Read[Read command buffer for leg N]
    Send[Send command frame to leg N]
    Poll[Wait for telemetry from leg N\nwith timeout]
    Write[Write telemetry buffer for leg N]
    Next{N = 6?}
    Inc[N = N + 1]

    Start --> Read --> Send --> Poll --> Write --> Next
    Next -- no --> Inc --> Read
    Next -- yes --> Start
```

If a leg does not respond within its timeout window, its telemetry buffer entry
is marked stale and the cycle advances. The rest of the legs are unaffected.

### 3.4 Information Flow

```mermaid
flowchart LR
    ControllerState[Normalized controller state]
    UserCmd[user_command_t]
    SchedState[Scheduler state\nphase + support/swing]
    FootTargets[Body-frame foot targets]
    JointDesired[Desired joint command set]
    JointLimited[Limited joint command set]
    CmdBuf[[Command buffer\nangles + arrival times]]
    TelBuf[[Telemetry buffer\ncurrent + status per leg]]

    Core[hex_controller_core]
    Loc[hex_locomotion]
    Lim[hex_motion_limits]
    Act[hex_actuation]
    Kin[hex_kinematics]
    RobotCfg[hex_robot_config]
    CfgMgr[hex_config_manager]
    RS485M[hex_rs485_master]

    Core --> ControllerState --> Loc
    Loc --> UserCmd --> Loc
    Loc --> SchedState --> Loc
    Loc --> FootTargets

    RobotCfg --> Loc
    Kin --> Loc
    CfgMgr --> RobotCfg
    Loc --> JointDesired

    JointDesired --> Lim
    CfgMgr --> Lim
    RobotCfg --> Lim
    Lim --> JointLimited

    JointLimited --> Act
    RobotCfg --> Act
    Act --> CmdBuf

    CmdBuf --> RS485M --> TelBuf --> Loc
```

The telemetry buffer feeds back into `hex_locomotion` so that ground contact
events detected from current spikes can influence gait decisions in real time.

---

## 4. Component Ownership Summary

### Motion And Robot Behavior

- `hex_locomotion`: high-level motion intent to desired body/leg targets.
  Unchanged from V1; reads telemetry buffer for ground contact data.
- `hex_motion_limits`: dynamic command conditioning and state estimation.
  Unchanged from V1.
- `hex_actuation`: writes joint commands to the shared command buffer.
  Same public API as V1; no PWM peripheral access in V2.
- `hex_kinematics`: reusable leg math. Unchanged from V1.
- `hex_robot_config`: geometry, mounts, calibration, and servo map projection.
  Unchanged from V1.

### RS485 Communication

- `hex_rs485_master`: exclusive RS485 bus owner; dedicated FreeRTOS task;
  per-leg send-and-poll cycle; writes telemetry buffer; handles per-leg
  timeouts.

### Input, Transport, And Control

Identical to V1. See common documentation.

### RPC, Configuration, And Shared Contracts

Identical to V1. See common documentation.

---

## 5. Dependency Rules

Rules inherited from V1:
- Motion components must not depend on transport-specific controller drivers.
- Transport drivers must not call locomotion modules directly.
- Kinematics code must remain usable without hardware dependencies.
- RPC command handling must rely on public controller/config APIs rather than
  internal transport details.

Additional V2 rules:
- **The 100 Hz locomotion loop must never block on leg communication.**
  `hex_actuation` writes to the command buffer and returns immediately. No
  motion stack component reads from or writes to the RS485 bus directly.
- **The RS485 bus is owned exclusively by `hex_rs485_master`.** No other
  component asserts DE, reads from, or writes to the RS485 UART peripheral.
- **Telemetry consumers must tolerate stale data.** The telemetry buffer holds
  the latest available result per leg; a consumer must not assume the entry was
  updated in the current loop cycle.
- **A single leg timing out must not stall the comm cycle.**
  `hex_rs485_master` applies a per-leg response timeout and advances to the
  next leg unconditionally.

---

## 6. Configuration Architecture Position

Identical to V1. Configuration is part of the runtime contract, not an
auxiliary subsystem. See
[../configuration/CONFIGURATION_PERSISTENCE_DESIGN.md](../configuration/CONFIGURATION_PERSISTENCE_DESIGN.md).

---

## 7. Documentation Map

- Hardware topology and board responsibilities: [HARDWARE_AND_MECHANICS.md](HARDWARE_AND_MECHANICS.md)
- RS485 protocol — packet format, framing, addressing, timing: [../interfaces/RS485_PROTOCOL.md](../interfaces/RS485_PROTOCOL.md) *(to be written)*
- LegBoard RP2040 firmware architecture: [../../firmware/leg/](../../firmware/leg/) *(to be written)*
- Configuration model: [../configuration/CONFIGURATION_PERSISTENCE_DESIGN.md](../configuration/CONFIGURATION_PERSISTENCE_DESIGN.md)
- RPC guide: [../interfaces/RPC_USER_GUIDE.md](../interfaces/RPC_USER_GUIDE.md)
- Hardware schematics: [../../hardware/README.md](../../hardware/README.md)

---

## 8. What This Document Does Not Cover

- Hardware board descriptions, power topology, and connector pinouts — see
  [HARDWARE_AND_MECHANICS.md](HARDWARE_AND_MECHANICS.md).
- RS485 packet format, framing, leg addressing, and timing budget — see
  [../interfaces/RS485_PROTOCOL.md](../interfaces/RS485_PROTOCOL.md).
- LegBoard (RP2040) firmware: interpolation model, watchdog behavior, PWM
  generation — see [`firmware/leg/`](../../firmware/leg/).
- Per-parameter configuration tables and namespace definitions — see common
  configuration docs.
- Forward-looking plans and feature backlog — see
  [../plans/TODO.md](../plans/TODO.md).
