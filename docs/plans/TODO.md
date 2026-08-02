# Hexapod Robot Project TODO List

## 🎯 Project Vision & New Goals

### Hardware Improvements
- [ ] **Touch sensors at each leg**
  - Research suitable force/pressure sensors for leg-ground contact detection
  - Design mounting system for sensors on leg tips
  - Implement sensor reading and processing
  - Integrate touch feedback into gait control system

- [ ] **Electronics & Power System Upgrade**
  - Replace/improve current power board to ESP32 connectors
  - Design more reliable electrical connections
  - Consider PCB design for better signal integrity
  - Implement proper power management and distribution

- [ ] **LegBoard total-current shunt resize (R4) for ~9A range on the total channel**
  - Today all four current-sense channels share identical hardware:
    INA4181A3 (100 V/V, fixed for the whole quad IC) + 10 mΩ Kelvin shunts
    (`hardware/legboard/legboard_sch.py` R2-R5), giving `I_max = Vref /
    (Gain×Rshunt) = 3.3V / (100×0.01Ω) ≈ 3.3A` per channel — fine for a
    single servo branch, but the `total` channel sums up to 3 branches and
    should have headroom toward ~9A under worst-case simultaneous stall
  - Plan: resize **R4 only** (total shunt) from 10 mΩ to ~3 mΩ →
    `I_max ≈ 11A`, keep R2/R3/R5 (coxa/femur/tibia branches) at 10 mΩ (gain
    is shared across the IC, so only the shunt value can differ per channel)
  - Requires: schematic/BOM edit via the hardware-schematics skill; physical
    rework (desolder/resolder R4) on any already-populated LegBoards; and a
    firmware change so `persist.cpp`'s default current-sense scale is
    per-channel (`total` ≈ 3.333 mA/mV, branches 1.0 mA/mV) instead of one
    shared `DEFAULT_ISENSE_MA_PER_MV` constant in `config.h`
  - Deferred until the next board order/rework pass. See
    `docs/development/LEG_CALIBRATION.md` "Hardware current range" for the
    derivation and current (~3.3A, all channels) status quo

- [ ] **Auto-generate the LegBoard ASCII diagram in `tools/leg_configurator.py`**
  - `BOARD_DIAGRAM` in `tools/leg_configurator.py` is hand-drawn (from
    `hardware/legboard/legboard_sch.py` placement +
    `hardware/legboard/board-front.png`), not derived from the real board
    render — fine for now (connectors don't move often) but will drift if the
    layout changes and nobody remembers to update it by hand
  - Idea: script `hardware/legboard/board-front.png` (or better, live
    placement data from `legboard_sch.py`) into a small ASCII/box-drawing
    renderer keyed by reference designator, so the diagram regenerates
    alongside the schematic instead of being maintained separately
  - Low priority / nice-to-have — plain text is manually accurate enough for
    now

- [ ] **IMU Integration for Terrain Leveling**
  - Add IMU sensor (accelerometer + gyroscope)
  - Implement body position/orientation estimation
  - Develop terrain leveling algorithms
  - Integrate IMU feedback into whole body control system

### Controller Systems
- [ ] **FlySky Controller Reconnection**
  - Debug signal stability issues caused by electronics interference
  - Implement proper signal filtering/shielding
  - Test and validate reliable communication
  - Add failsafe mechanisms

- [ ] **ESP-NOW Controller Implementation**
  - Design ESP-NOW communication protocol
  - Implement controller device (separate ESP32)
  - Add wireless communication handling
  - Create user interface for ESP-NOW controller

### Vision & Processing
- [ ] **Jetson Nano Integration**
  - Add Jetson Nano compute module to the system
  - Design mounting and power solution
  - Implement dual camera system
  - Develop computer vision capabilities
  - Create communication bridge between ESP32 and Jetson Nano

### Configuration & Tuning Portal
- [ ] **Hexapod Configurator (Betaflight-inspired)**
  - Design web-based configuration portal for robot tuning
  - Implement RPC-like communication protocol between configurator and ESP32
  - Create real-time parameter adjustment interface
  - Add live telemetry and monitoring capabilities
  - Implement configuration backup/restore functionality

### Bio-Inspired & Research-Backed Locomotion

These items upgrade the open-loop phase-table gait engine
(`gait_scheduler.c` → `swing_trajectory.c` → `whole_body_control.c` → `kpp_system.c`)
toward biologically-grounded, continuously-adaptive locomotion. Each item is
anchored to published research. Tiers reflect hardware dependency, not priority.

#### Tier 1 — Pure software (no new hardware; works on v1)
- [ ] **Wilson gait continuum (continuously-variable duty factor)**
  - Replace the 3 discrete gaits (tripod/ripple/wave) and their hardcoded duty
    factors (`S` in `swing_trajectory.c:60-65`) with a single continuum where
    duty factor `β` decreases with commanded speed (slow→wave, fast→tripod)
  - Derive per-leg phase offsets from one wave parameter in `gait_scheduler.c`
  - Replaces the "crude frequency scaling" TODO (`gait_scheduler.c:22-26`)
  - Highest effort/payoff ratio; foundation for the CPG item below
  - Ref: Wilson, D.M. (1966), "Insect Walking", *Annual Review of Entomology* 11:103-122
  - Ref: Graham, D. (1985), "Pattern and Control of Walking in Insects", *Advances in Insect Physiology* 18:31-140

- [ ] **CPG gait engine — coupled phase oscillators**
  - Replace the phase *table* in `gait_scheduler.c` with 6 coupled oscillators
    (Kuramoto / Hopf) whose phase-locking encodes the gait; preserve the existing
    `phase` / `leg_states` output contract so downstream stages are unchanged
  - Enables smooth online gait transitions (no popping at speed thresholds) and
    automatic re-synchronization after disturbance
  - Keep phase tables behind a config flag as fallback
  - Natural injection point for sensory feedback (see Tegotae, Tier 3)
  - Ref: Ijspeert, A.J. (2008), "Central pattern generators for locomotion control in animals and robots: a review", *Neural Networks* 21(4):642-653
  - Ref: Righetti & Ijspeert (2008), "Pattern generators with sensory feedback for the control of quadruped locomotion", *IEEE ICRA*

- [ ] **Minimum-impact swing foot trajectory (zero-velocity touchdown)**
  - Current swing uses a sine vertical arc but a *linear* horizontal sweep
    (`x_rel = (-0.5+tau)*L`, `swing_trajectory.c:98`), causing instantaneous
    horizontal velocity reversal at touchdown → foot slip and servo shock
  - Replace horizontal sweep with a quintic polynomial / Bézier giving zero
    foot velocity and acceleration at lift-off and touchdown
  - Overlaps existing "minimum-jerk / quintic foot trajectory" swing TODO
  - Ref: Raibert, M. (1986), *Legged Robots That Balance*, MIT Press
  - Ref: Park, Wensing & Kim (2017), "High-speed bounding with the MIT Cheetah 2", *IJRR* 36(2):167-192 (swing-leg trajectory design)

- [ ] **Holonomic (omnidirectional) body velocity — lateral `vy` + crab walk**
  - `user_command_t` currently carries only `vx` (forward) and `wz` (yaw); the
    robot cannot strafe. Add a lateral `vy` term and compose the full planar
    twist (vx, vy, ω) into per-leg stride vectors
  - Unlocks crab-walking and motion along any heading; hexapods are naturally
    holonomic so this is low-risk
  - Localized to `swing_trajectory.c` (stride vector) and one field in
    `user_command.h`; lowest-effort visible win in this section
  - Ref: standard omnidirectional legged locomotion; e.g. Gonzalez de Santos,
    Garcia & Estremera (2006), *Quadrupedal Locomotion: An Introduction to the
    Control of Four-legged Robots*, Springer (body-velocity → foot-stride mapping)

#### Tier 2 — Needs v2 IMU (small code once hardware lands)
- [ ] **Body-attitude stabilization (auto-leveling on slopes)**
  - Use IMU roll/pitch to add per-leg z offsets keeping the body level on
    uneven terrain; injected at the trajectory→WBC stage (the roll/pitch
    `pose_mode` TODO at `swing_trajectory.c:143`)
  - Ref: Saranli, Buehler & Koditschek (2001), "RHex: A Simple and Highly Mobile Hexapod Robot", *IJRR* 20(7):616-631
  - Ref: Pratt et al. (2001), "Virtual Model Control: An Intuitive Approach for Bipedal Locomotion", *IJRR* 20(2):129-143

- [ ] **Inertial stumble / elevator reflex**
  - On unexpected IMU pitch/roll spike (or a swing leg failing to find ground),
    trigger an elevator reflex: lift higher and re-place
  - Ref: Cruse, Kindermann, Schumm, Dean & Schmitz (1998), "Walknet — a biologically inspired network to control six-legged walking", *Neural Networks* 11(7-8):1435-1447

- [ ] **Self-righting / fall recovery (third priority)**
  - Detect an inverted or fallen state from the IMU, then run a scripted (or
    optimized) flip-back-over routine to return to a walkable posture
  - Lower priority than auto-leveling and reflexes; great demo once those land
  - Ref: Saranli, Buehler & Koditschek (2001), "RHex: A Simple and Highly Mobile Hexapod Robot", *IJRR* 20(7):616-631
  - Ref: Saranli & Koditschek (2002), "Template-based control of the RHex hexapod", *IEEE ICRA*

#### Tier 3 — Needs v2 per-leg current sensing (flagship research feature)
- [ ] **Tegotae — decentralized force-feedback gait emergence**
  - Each leg modulates its own oscillator phase by the ground reaction force it
    feels (using per-servo current draw as a GRF proxy); speed-dependent gaits
    and tripod↔wave transitions emerge with no central gait selection
  - Plugs the force term directly into the CPG engine (Tier 1)
  - Ref: Owaki & Ishiguro (2017), "A Minimal Model Describing Hexapedal Interlimb Coordination: The Tegotae-Based Approach", *Frontiers in Neurorobotics* 11:29
  - Ref: Owaki, Kano, Nagasawa, Tero & Ishiguro (2013), "Simple robot suggests physical interlimb communication is essential for quadruped walking", *J. Royal Society Interface* 10:20120669

- [ ] **Walknet — full decentralized coordination rules (lowest priority)**
  - The complete set of ~6 local leg-to-leg coordination rules from which insect
    gaits emerge with no central scheduler; kinematic cousin of Tegotae (which is
    force-based)
  - **Likely cannot run concurrently with Tegotae** — both are decentralized
    interlimb-coordination schemes competing for the same role. Treat as an
    **A/B-toggle alternative** to Tegotae (config-selectable), not an addition
  - Lowest priority in this section; keep on the list as a research comparison
  - Ref: Cruse, Kindermann, Schumm, Dean & Schmitz (1998), "Walknet — a biologically inspired network to control six-legged walking", *Neural Networks* 11(7-8):1435-1447

#### Needs Analysis — open questions before scheduling
These are promising but not yet scoped; each needs analysis of how it fits with
the items above before it earns a tier/priority.
- [ ] **Static stability margin + support-polygon-aware foot placement**
  - Compute the support polygon from stance-leg positions and keep the CoM
    projection inside it with a margin; bias stance / delay lift-off when the
    margin shrinks
  - **Open question:** implications and how it interacts with the CPG, Tegotae,
    and holonomic motion — is it a safety gate layered on top, or a placement
    planner that competes with them? Needs analysis before scheduling
  - Ref: McGhee & Frank (1968), "On the stability properties of quadruped creeping gaits", *Mathematical Biosciences* 3:331-351
  - Ref: Hirose, Tsukagoshi & Yoneda (1998), "Normalized Energy Stability Margin", *IEEE ICRA*

- [ ] **Manipulability- / singularity-aware posture optimization**
  - Choose body height and stance width to keep each leg's Jacobian
    well-conditioned (away from joint limits and singularities)
  - **Open question:** how it composes with auto-leveling, holonomic motion, and
    the gait engine (shared posture authority). Needs analysis before scheduling
  - Ref: Yoshikawa (1985), "Manipulability of Robotic Mechanisms", *IJRR* 4(2):3-9

- [ ] **Phase-resetting sensory entrainment into the CPG**
  - Feed contact events back to reset oscillator phase so the gait rhythm
    entrains to the terrain; the proven bridge between feedforward CPG and
    feedback reflexes
  - **Open question:** whether/how it fits alongside Tegotae's phase modulation
    (both inject sensory feedback into the same oscillators). Needs analysis,
    depends on the CPG engine existing first
  - Ref: Aoi & Tsuchiya (2005), "Locomotion control of a biped robot using nonlinear oscillators", *Autonomous Robots* 19(3):219-232

---

## 🔧 Existing Code TODOs

### Configuration & Storage
- [x] **NVS Storage Foundation** (`hex_config_manager`, `hex_motion_limits`, `robot_config`)
  - Implemented namespace-based NVS persistence for `system`, `joint_cal`, `leg_geom`, and `motion_lim`
  - Implemented boot-time load/default/migration pipeline in config manager
  - Implemented KPP runtime consumption of `motion_lim` during `kpp_init` with fail-fast behavior

- [x] **NVS Scope Expansion**
  - Extend persisted namespaces to `servo_map`, `controller`, `gait` and `wifi` as needed
  - Add explicit coverage tracking for remaining parameter schemas

- [x] **Per-leg Configuration** (`robot_config.c`)
  - Replace geometry/mount/stance hardcoded defaults with storage-loaded values when `leg_geom` is loaded
  - Consider per-leg geometry differences (mirrors, tolerances)
  - Remove remaining fallback paths once strict source-of-truth policy is finalized (`joint_cal` still has fallback defaults)

### Motion Control System
- [ ] **Advanced Motion Modes** (`kpp_system.h`)
  - Add MOTION_MODE_FAST for quick reactions
  - Add MOTION_MODE_EMERGENCY for fall recovery
  - Add mode_transition_time for multi-mode support
  - Implement fast mode limits
  - Implement emergency mode limits

- [ ] **Swing Trajectory Improvements** (`swing_trajectory.c`, `swing_trajectory.h`)
  - Make swing trajectory parameters configurable via Kconfig or runtime config
  - Make max turn angle configurable (currently hardcoded to 0.4 rad)
  - Make turn direction configurable
  - Add simple body roll/pitch offsets when pose_mode is active
  - Add yaw (wz) coupling to bias per-leg x/y for turning
  - Consider exposing trajectory parameters via config or calibration struct
  - Add C1/C2-continuous swing/stance blending to reduce velocity/acceleration discontinuities at phase boundaries
  - Add minimum-jerk or quintic foot trajectory mode for smoother liftoff/touchdown
  - Add configurable easing windows around liftoff/touchdown events

- [ ] **Gait Scheduler Enhancements** (`gait_scheduler.c`)
  - Consider separate forward/turning components and modulate phase rate
  - Replace current groupings with well-defined groupings and exact phase windows per gait
  - Add commanded-speed slew limiting so phase/frequency changes are gradual
  - Add gait transition blending (tripod/ripple/wave) to avoid abrupt group switching

### Control & Safety
- [ ] **Error Handling & Safety** (`whole_body_control.c`)
  - Add error signaling so robot_control can engage a safe stop
  - Implement comprehensive safety mechanisms
  - Add system health monitoring
  - Add arming checks (controller link valid, config loaded, telemetry channel healthy, basic power checks)
  - Add parameter-change safe-apply flow with timeout revert for risky live tuning updates

- [x] **Controller Configuration** (`controller.c`)
  - Make DEAD_BAND configurable via Kconfig or runtime configuration
  - Add controller parameter tuning interfaces

### Future Sensor Integration
- [ ] **Coordinate Frame Transformations** (`kpp_config.h`)
  - Add coordinate frame transformation parameters
  - Implement sensor fusion framework

- [ ] **Sensor Fusion Parameters** (`kpp_config.h`)
  - Design sensor fusion architecture for IMU/force sensors
  - Implement sensor data processing pipeline
  - Add sensor calibration procedures

---

## 🖥️ Hexapod Configurator Portal (Betaflight-inspired)

### Core Infrastructure
- [ ] **Communication Protocol**
  - Design JSON-RPC or MessagePack-based protocol for ESP32 ↔ Configurator
  - Implement WebSocket connection for real-time communication
  - Add command/response handling with error codes
  - Create parameter read/write API endpoints
  - Implement telemetry streaming protocol
  - Split channels into command/control plane and telemetry plane to avoid tuning latency under load
  - Add topic-based telemetry subscription with configurable publish rates

- [ ] **Web Application Framework**
  - Create React/Vue.js based configurator interface
  - Design responsive UI for desktop and tablet use
  - Implement WebSocket client for ESP32 communication
  - Add configuration state management
  - Create reusable UI components for parameter tuning

### Configuration Pages & Features

#### 🦿 **Leg Configuration Tab**
- [ ] Individual leg parameter tuning
  - Per-leg servo angle offsets and limits
  - Leg geometry parameters (coxa, femur, tibia lengths)
  - Servo direction and calibration settings
  - Real-time leg position visualization (3D model)
  - "Test Leg Movement" controls for each leg

#### ⚙️ **Motion Control Tab**
- [ ] Gait pattern configuration
  - Gait type selection (tripod, wave, ripple, custom)
  - Step height, length, and timing parameters
  - Swing trajectory curve parameters
  - Body height and stance width settings
  - Real-time gait visualization

#### 🎮 **Controller Settings Tab**
- [ ] Input device configuration
  - FlySky controller channel mapping and calibration
  - ESP-NOW controller pairing and setup
  - Dead zones and sensitivity curves
  - Control mode switching (manual/autonomous)
  - Input signal monitoring and diagnostics

#### 🧭 **Sensors & IMU Tab**
- [ ] Sensor calibration and monitoring
  - IMU calibration wizard (accelerometer, gyroscope)
  - Touch sensor threshold settings per leg
  - Real-time sensor data visualization
  - Terrain leveling algorithm parameters
  - Sensor fusion weight settings

#### 🔧 **System Parameters Tab**
- [ ] Core system configuration
  - KPP motion limits (velocity, acceleration, jerk)
  - Safety parameters and emergency stops
  - Power management settings
  - Debug logging levels
  - System health monitoring

#### 📊 **Telemetry & Monitoring Tab**
- [ ] Real-time robot status
  - Live 3D robot visualization showing current pose
  - Servo positions and load monitoring
  - Battery voltage and current consumption
  - System temperature monitoring
  - Error logs and diagnostic information
  - Per-joint plots: commanded vs limited angle/velocity/acceleration
  - Gait plots: phase, support/swing state, foot target XYZ per leg
  - Control-loop timing charts (dt jitter, loop overrun counters)

#### 💾 **Configuration Management Tab**
- [ ] Profile and backup management
  - Save/load configuration profiles
  - Export/import settings to/from files
  - Factory reset functionality
  - Configuration versioning and diff viewing
  - Automatic backup before major changes

### Advanced Features
- [ ] **Live Tuning & Testing**
  - Real-time parameter adjustment with immediate effect
  - "Safe Mode" with automatic revert after timeout
  - Parameter validation and bounds checking
  - Undo/redo functionality for parameter changes
  - Batch parameter updates

- [ ] **Calibration Wizards**
  - Automated servo calibration sequence
  - IMU calibration step-by-step guide
  - Leg geometry measurement assistant
  - Gait optimization wizard
  - Touch sensor threshold auto-calibration

- [ ] **Diagnostic Tools**
  - Servo health check and performance testing
  - Communication link quality monitoring
  - System performance profiling
  - Error code lookup and troubleshooting guide
  - Log file viewer and analysis
  - Blackbox-style ring buffer capture with event markers (failsafe, IK error, timeout)
  - Export log bundles for offline plotting/comparison between tuning sessions
  - Pre/post event capture windows for fault analysis

### ESP32 Firmware Support
- [ ] **Parameter Storage System**
  - Extend NVS storage to support all configurable parameters
  - Implement parameter validation and bounds checking
  - Add parameter change notification system
  - Create parameter schema/metadata system
  - Implement atomic parameter updates

- [ ] **Web Server & API**
  - Implement HTTP server for configurator hosting
  - Add WebSocket server for real-time communication
  - Create REST API for parameter access
  - Implement file upload/download for configurations
  - Add OTA update support through configurator

- [ ] **Telemetry System**
  - Real-time data streaming to configurator
  - Configurable telemetry rates and data selection
  - Data logging to SD card or flash memory
  - Remote logging capabilities
  - Performance metrics collection
  - Binary telemetry encoding (MessagePack/CBOR) to reduce bandwidth and latency vs text logs
  - Per-topic rate limiting and backpressure handling to protect control-loop timing
  - Runtime telemetry on/off and topic filtering without reboot

---

## 📋 Implementation Priority

### Phase 1: Foundation (High Priority)
1. FlySky controller debugging and reconnection
2. Electronics/connector improvements
3. Expand NVS coverage for remaining namespaces (foundation is complete)
4. Error handling and safety mechanisms
5. **Basic configurator infrastructure** (communication protocol, web framework)
6. **High-rate WiFi telemetry path** (non-serial observability for tuning)

### Phase 2: Enhanced Control & Configuration (Medium Priority)
1. **Core configurator pages** (Leg Config, Motion Control, System Parameters)
2. IMU integration and terrain leveling
3. Touch sensors implementation
4. Advanced motion modes (Fast/Emergency)
5. Swing trajectory improvements
6. **Configuration management and calibration wizards**
7. **Gait transition smoothing + phase/frequency slew limiting**
8. **Bio-inspired locomotion Tier 1** (Wilson continuum → CPG oscillators → min-impact swing → holonomic/lateral `vy`) — pure software, no new hardware

### Phase 3: Advanced Features (Lower Priority)
1. **Advanced configurator features** (telemetry, diagnostics, live tuning)
2. ESP-NOW controller development
3. Jetson Nano integration
4. Dual camera system
5. Computer vision capabilities
6. Advanced sensor fusion
7. **Blackbox-style event logging and offline analysis workflow**
8. **Bio-inspired locomotion Tier 2/3** (IMU attitude stabilization + reflexes; self-righting/flip recovery; Tegotae force-feedback gaits once per-leg current sensing exists. Walknet is an A/B alternative to Tegotae, lowest priority)
9. **Locomotion "Needs Analysis" items** (static stability margin, singularity-aware posture, CPG phase-resetting entrainment) — scope and fit before scheduling

---

## 📝 Notes
- Current system has comprehensive KPP (Kinematic Pose Position) implementation
- Debug infrastructure is in place for development and tuning
- Motion limiting and safety systems are partially implemented
- FlySky controller was previously working but needs debugging for signal stability

---

*Last updated: June 16, 2026*
