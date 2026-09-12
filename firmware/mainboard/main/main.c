/*
 *
 * License: Apache-2.0
 */

#include <stdint.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h"
#include "leg.h"
#include <math.h>
#include "gait_scheduler.h"
#include "swing_trajectory.h"
#include "whole_body_control.h"
#include "robot_control.h"
#include "robot_config.h"
#include "user_command.h"
#include "controller.h"
#include "controller_flysky_ibus.h"
#include <string.h>
#include "wifi_ap.h"
#include "controller_bt_classic.h"
#include "controller_wifi_tcp.h"
#include "kpp_system.h"
#include "config_manager_runtime_api.h"
#include "config_ns_system_api.h"
#include "config_ns_controller_api.h"
#include "config_ns_wifi_api.h"
#include "config_ns_gait_api.h"
#include "rpc_commands.h"
#include "rs485_master.h"

static const char *TAG = "main";

// --- Gait Framework Main Loop ---
void gait_framework_main(void *arg)
{
    gait_scheduler_t scheduler;
    swing_trajectory_t trajectory;
    whole_body_cmd_t cmds;
    user_command_t ucmd; // current command
    user_command_t prev_cmd; // previous command for change detection
    memset(&ucmd, 0, sizeof(ucmd));
    memset(&prev_cmd, 0, sizeof(prev_cmd));

    // Initialize KPP (Kinematic Pose Position) system
    kinematic_state_t kpp_state;
    motion_limits_t motion_limits;
    ESP_ERROR_CHECK(kpp_init(&kpp_state, &motion_limits));
    ESP_LOGI(TAG, "KPP system initialized with motion limiting enabled");

    // Initialize gait framework modules
    const gait_config_t* gait_cfg = config_get_gait();
    if (!gait_cfg) {
        ESP_LOGE(TAG, "Gait namespace unavailable");
        return;
    }

    gait_scheduler_init(&scheduler, gait_cfg->cycle_time_s);
    scheduler.duty_factor_min = gait_cfg->duty_factor_min;
    scheduler.duty_factor_max = gait_cfg->duty_factor_max;
    swing_trajectory_init(&trajectory, gait_cfg->step_length_m, gait_cfg->clearance_height_m);
    trajectory.y_range_m = gait_cfg->y_range_m;
    trajectory.z_min_m = gait_cfg->z_min_m;
    trajectory.z_max_m = gait_cfg->z_max_m;
    trajectory.max_yaw_per_cycle_rad = gait_cfg->max_yaw_per_cycle_rad;
    trajectory.turn_direction = gait_cfg->turn_direction;

    // Nominal loop cadence. dt is measured per-iteration below (see loop) so the
    // gait clock tracks wall time even if a cycle overruns; this is only the
    // scheduling period fed to vTaskDelayUntil.
    const TickType_t loop_period_ticks = pdMS_TO_TICKS(10); // 100 Hz target
    ESP_LOGI(TAG, "gait loop: FreeRTOS tick %d Hz (portTICK_PERIOD_MS=%d), period %d tick(s); task core %d prio %d",
             (int)configTICK_RATE_HZ, (int)portTICK_PERIOD_MS, (int)loop_period_ticks,
             (int)xPortGetCoreID(), (int)uxTaskPriorityGet(NULL));
    if (loop_period_ticks < 1) {
        ESP_LOGW(TAG, "tick rate too low for a sub-10ms period; loop will run slower than 100 Hz");
    }

    TickType_t last_wake_ticks = xTaskGetTickCount();
    int64_t prev_cycle_us = esp_timer_get_time();

    // --- Loop-rate diagnostics ---------------------------------------------
    // Accumulate per-iteration timing and dump a summary once every 5 s:
    //   - busy:   compute time of one iteration (locomotion -> actuation), no wait
    //   - period: wall time between consecutive iteration starts (actual cadence)
    // plus the RS485 bus master's own sweep/transaction counters, so leg update
    // rate and main-loop rate can be compared side by side.
    const int64_t DIAG_WINDOW_US = 5 * 1000 * 1000;
    int64_t diag_window_start_us = esp_timer_get_time();
    int64_t diag_prev_iter_us    = diag_window_start_us;
    uint32_t diag_iters          = 0;
    int64_t  diag_busy_total_us  = 0;
    int64_t  diag_period_total_us = 0;
    uint32_t diag_busy_min_us    = UINT32_MAX, diag_busy_max_us   = 0;
    uint32_t diag_period_min_us  = UINT32_MAX, diag_period_max_us = 0;

    // Per-phase breakdown of the "busy" time, so a slow stage can be spotted.
    enum { PH_POLL, PH_SCHED, PH_SWING, PH_WBC, PH_KPPLIM, PH_EXEC, PH_KPPUPD, PH_COUNT };
    static const char *const ph_name[PH_COUNT] = {
        "poll", "sched", "swing", "wbc", "kpp_lim", "exec", "kpp_upd"
    };
    int64_t  ph_total_us[PH_COUNT] = {0};
    uint32_t ph_max_us[PH_COUNT]   = {0};
    #define DIAG_PHASE(idx, stmt) do {                                  \
        int64_t _p0 = esp_timer_get_time();                             \
        stmt;                                                           \
        int64_t _pd = esp_timer_get_time() - _p0;                       \
        ph_total_us[idx] += _pd;                                        \
        if (_pd > ph_max_us[idx]) ph_max_us[idx] = (uint32_t)_pd;       \
    } while (0)

    while (1) {
        int64_t iter_start_us = esp_timer_get_time();

        // Measured timestep since the previous iteration. Clamped so a one-off
        // stall (WiFi association, logging burst) can't fast-forward the gait or
        // blow up the KPP velocity/accel estimates via a huge 1/dt.
        float dt = (float)(iter_start_us - prev_cycle_us) / 1e6f;
        prev_cycle_us = iter_start_us;
        if (dt < 0.001f) dt = 0.001f;
        if (dt > 0.05f)  dt = 0.05f;

        // Copy current to previous then poll new
        prev_cmd = ucmd;
        DIAG_PHASE(PH_POLL, user_command_poll(&ucmd));
        if (!controller_user_command_equal(&prev_cmd, &ucmd, 1e-2f)) {
            ESP_LOGI(TAG, "User command: vx=%.2f, vy=%.2f, wz=%.2f, z_target=%.2f, y_offset=%.2f gait=%d enable=%d pose=%d terrain=%d step_scale=%.2f",
                    ucmd.vx, ucmd.vy, ucmd.wz, ucmd.z_target, ucmd.y_offset, ucmd.gait, ucmd.enable, ucmd.pose_mode, ucmd.terrain_climb, ucmd.step_scale);
        }

        whole_body_cmd_t limited_cmds;
        // Update gait scheduler (leg phases)
        DIAG_PHASE(PH_SCHED, gait_scheduler_update(&scheduler, dt, &ucmd));
        // Generate swing trajectories for legs using scheduler + command
        DIAG_PHASE(PH_SWING, swing_trajectory_generate(&trajectory, &scheduler, &ucmd));
        // Compute joint commands from trajectories
        DIAG_PHASE(PH_WBC, whole_body_control_compute(&trajectory, &cmds));
        // KPP: Apply motion limiting for smooth servo operation
        DIAG_PHASE(PH_KPPLIM, kpp_apply_limits(&kpp_state, &motion_limits, &cmds, &limited_cmds, dt));
        // Send limited commands to robot
        DIAG_PHASE(PH_EXEC, robot_execute(&limited_cmds));
        // CRITICAL FIX: Update state estimation based on ORIGINAL commands, not limited ones
        // This breaks the feedback loop that was causing oscillations
        DIAG_PHASE(PH_KPPUPD, kpp_update_state(&kpp_state, &cmds, dt));  // original commands for state estimation

        // --- Loop-rate diagnostics: accumulate this iteration ---
        int64_t iter_busy_us = esp_timer_get_time() - iter_start_us;
        int64_t iter_period_us = iter_start_us - diag_prev_iter_us;
        diag_prev_iter_us = iter_start_us;
        diag_iters++;
        diag_busy_total_us += iter_busy_us;
        if (iter_busy_us < diag_busy_min_us) diag_busy_min_us = (uint32_t)iter_busy_us;
        if (iter_busy_us > diag_busy_max_us) diag_busy_max_us = (uint32_t)iter_busy_us;
        if (diag_iters > 1) { // first period spans init, skip it
            diag_period_total_us += iter_period_us;
            if (iter_period_us < diag_period_min_us) diag_period_min_us = (uint32_t)iter_period_us;
            if (iter_period_us > diag_period_max_us) diag_period_max_us = (uint32_t)iter_period_us;
        }

        if (iter_start_us - diag_window_start_us >= DIAG_WINDOW_US) {
            // Stats keep accumulating every iteration; only the 5 s dump below is
            // gated off. Emitting these lines pushes ~300 bytes over the 115200
            // console UART, which blocks this task for 10-20 ms and shows up as a
            // period spike. Flip DIAG_LOG_ENABLED to 1 for a bring-up session.
            #define DIAG_LOG_ENABLED 0

            // Drain the bus counters regardless, so they don't grow unbounded.
            rs485_master_stats_t bus;
            rs485_master_get_stats(&bus);

            if (DIAG_LOG_ENABLED) {
                float win_s = (iter_start_us - diag_window_start_us) / 1e6f;
                uint32_t pcount = (diag_iters > 1) ? (diag_iters - 1) : 1;
                ESP_LOGI(TAG,
                    "loop: %.1f Hz (%lu iters/%.1fs) | busy avg %.2f max %.2f min %.2f ms | period avg %.2f max %.2f min %.2f ms",
                    diag_iters / win_s, (unsigned long)diag_iters, win_s,
                    diag_busy_total_us / 1000.0f / diag_iters,
                    diag_busy_max_us / 1000.0f, diag_busy_min_us / 1000.0f,
                    diag_period_total_us / 1000.0f / pcount,
                    diag_period_max_us / 1000.0f, diag_period_min_us / 1000.0f);

                if (bus.sweeps > 0) {
                    uint32_t ok = 0, to = 0;
                    for (int i = 0; i < NUM_LEGS; ++i) { ok += bus.leg_ok[i]; to += bus.leg_timeout[i]; }
                    ESP_LOGI(TAG,
                        "bus: %.1f leg-updates/s (%lu sweeps) | sweep avg %.2f max %.2f min %.2f ms | txn avg %.0f max %lu min %lu us | ok %lu timeout %lu",
                        bus.sweeps / win_s, (unsigned long)bus.sweeps,
                        bus.sweep_total_us / 1000.0f / bus.sweeps,
                        bus.sweep_max_us / 1000.0f, bus.sweep_min_us / 1000.0f,
                        bus.txn_count ? (float)bus.txn_total_us / bus.txn_count : 0.0f,
                        (unsigned long)bus.txn_max_us, (unsigned long)bus.txn_min_us,
                        (unsigned long)ok, (unsigned long)to);
                    if (to > 0) {
                        ESP_LOGW(TAG,
                            "bus timeouts per leg: L1=%lu L2=%lu L3=%lu L4=%lu L5=%lu L6=%lu",
                            (unsigned long)bus.leg_timeout[0], (unsigned long)bus.leg_timeout[1],
                            (unsigned long)bus.leg_timeout[2], (unsigned long)bus.leg_timeout[3],
                            (unsigned long)bus.leg_timeout[4], (unsigned long)bus.leg_timeout[5]);
                    }
                } else {
                    ESP_LOGW(TAG, "bus: RS485 master completed no sweeps this window (task stalled?)");
                }

                char phbuf[192];
                int off = 0;
                for (int p = 0; p < PH_COUNT; ++p) {
                    off += snprintf(phbuf + off, sizeof(phbuf) - off, "%s%s %.2f/%.2f",
                                    p ? " | " : "", ph_name[p],
                                    ph_total_us[p] / 1000.0f / diag_iters,
                                    ph_max_us[p] / 1000.0f);
                    if (off >= (int)sizeof(phbuf)) break;
                }
                ESP_LOGI(TAG, "phase avg/max ms: %s", phbuf);
            }
            (void)bus;

            diag_window_start_us = iter_start_us;
            diag_iters = 0;
            diag_busy_total_us = diag_period_total_us = 0;
            diag_busy_min_us = diag_period_min_us = UINT32_MAX;
            diag_busy_max_us = diag_period_max_us = 0;
            memset(ph_total_us, 0, sizeof(ph_total_us));
            memset(ph_max_us, 0, sizeof(ph_max_us));
        }

        // Hold a fixed cadence. If the cycle overran the period, xTaskDelayUntil
        // returns immediately (pdFALSE) without blocking -- in that case yield one
        // tick anyway so the idle task runs and the task watchdog stays happy.
        if (xTaskDelayUntil(&last_wake_ticks, loop_period_ticks) == pdFALSE) {
            last_wake_ticks = xTaskGetTickCount();
            vTaskDelay(1);
        }
    }
}

void app_main(void)
{
    // Initialize configuration manager first
    ESP_LOGI(TAG, "Starting hexapod application...");
    esp_err_t err = config_manager_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize configuration manager: %s", esp_err_to_name(err));
        return;
    }
    
    // Get system configuration
    const system_config_t* sys_config = config_get_system();
    ESP_LOGI(TAG, "System config loaded: robot_id=%s, robot_name=%s", 
             sys_config->robot_id, sys_config->robot_name);
    ESP_LOGI(TAG, "Safety settings: emergency_stop=%s, voltage_min=%.2fV", 
             sys_config->emergency_stop_enabled ? "enabled" : "disabled",
             sys_config->safety_voltage_min);
    
    // Joint calibration system is now active and will be used by robot_control
    ESP_LOGI(TAG, "Joint calibration configuration loaded and ready for robot control");
    
    // Apply startup delay from configuration
    if (sys_config->startup_delay_ms > 0) {
        ESP_LOGI(TAG, "Applying startup delay: %lu ms", (unsigned long)sys_config->startup_delay_ms);
        vTaskDelay(pdMS_TO_TICKS(sys_config->startup_delay_ms));
    }
    
    // Initialize RPC system
    rpc_init();

    // Load WiFi namespace-backed options for AP and TCP controller transport.
    const wifi_config_namespace_t* wifi_ns_cfg = config_get_wifi();
    if (!wifi_ns_cfg) {
        ESP_LOGE(TAG, "WiFi namespace unavailable");
        return;
    }

    wifi_ap_options_t ap_opts = {
        .mode = (wifi_ap_ssid_mode_e)wifi_ns_cfg->ap_ssid_mode,
        .fixed_prefix = wifi_ns_cfg->ap_fixed_prefix,
        .fixed_ssid = wifi_ns_cfg->ap_fixed_ssid,
        .password = wifi_ns_cfg->ap_password,
        .channel = (uint8_t)wifi_ns_cfg->ap_channel,
        .max_clients = (uint8_t)wifi_ns_cfg->ap_max_clients,
    };
    
    // Bring up WiFi AP early so that network-based controller drivers or diagnostics
    // can connect even if later initialization stalls.
    if (!wifi_ap_init_with_options(&ap_opts)) {
        ESP_LOGE(TAG, "Failed to initialize WiFi AP from namespace configuration");
        return;
    }
    
    // Initialize robot configuration (namespace-backed; fail fast on invalid state)
    err = robot_config_init_default();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize robot configuration: %s", esp_err_to_name(err));
        return;
    }

    // V2: start the RS485 bus master. It owns UART2 and polls the six LegBoards
    // in its own task, independent of the 100 Hz motion loop. Actuation writes
    // desired joint angles into its command buffer (non-blocking).
    err = rs485_master_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize RS485 master: %s", esp_err_to_name(err));
        return;
    }
    
    // Initialize the primary controller from configuration namespaces.
    const controller_config_namespace_t* controller_cfg = config_get_controller();
    if (!controller_cfg) {
        ESP_LOGE(TAG, "Controller namespace unavailable");
        return;
    }

    controller_config_t ctrl_cfg = {
        .driver_type = controller_cfg->driver_type,
        .task_stack = (int)controller_cfg->task_stack,
        .task_prio = (int)controller_cfg->task_priority,
        .driver_cfg = NULL,
        .driver_cfg_size = 0,
    };

    // Assign driver-specific configuration if needed. static: their address is
    // stored in controller_core (g_cfg.driver_cfg) and read later by the driver
    // task; app_main returns once the tasks are spawned, so an auto (stack)
    // struct here would dangle.
    static controller_flysky_ibus_cfg_t flysky_cfg;
    static controller_wifi_tcp_cfg_t wifi_tcp_cfg;
    if (ctrl_cfg.driver_type == CONTROLLER_DRIVER_FLYSKY_IBUS) {
        flysky_cfg.uart_port = controller_cfg->flysky_uart_port;
        flysky_cfg.tx_gpio = controller_cfg->flysky_tx_gpio;
        flysky_cfg.rx_gpio = controller_cfg->flysky_rx_gpio;
        flysky_cfg.rts_gpio = controller_cfg->flysky_rts_gpio;
        flysky_cfg.cts_gpio = controller_cfg->flysky_cts_gpio;
        flysky_cfg.baud_rate = controller_cfg->flysky_baud_rate;
        ctrl_cfg.driver_cfg = &flysky_cfg;
        ctrl_cfg.driver_cfg_size = sizeof(flysky_cfg);
    } else if (ctrl_cfg.driver_type == CONTROLLER_DRIVER_WIFI_TCP) {
        wifi_tcp_cfg.listen_port = (uint16_t)wifi_ns_cfg->tcp_listen_port;
        wifi_tcp_cfg.connection_timeout_ms = (uint16_t)wifi_ns_cfg->tcp_connection_timeout_ms;
        ctrl_cfg.driver_cfg = &wifi_tcp_cfg;
        ctrl_cfg.driver_cfg_size = sizeof(wifi_tcp_cfg);
    }
    
    controller_init(&ctrl_cfg);

    // Launch the primary controller's input driver. controller_init() only
    // stores config -- without an explicit launch here the FlySky iBUS UART
    // reader task never starts, so no stick input reaches locomotion. (The
    // dispatch that used to do this lived in user_command_init(), which
    // nothing calls.) Only the iBUS driver is wired up here; WIFI_TCP is
    // handled by the always-on secondary interface below.
    if (ctrl_cfg.driver_type == CONTROLLER_DRIVER_FLYSKY_IBUS) {
        controller_driver_init_flysky_ibus(&ctrl_cfg);
    }

    // ALWAYS initialize WiFi TCP controller for RPC commands (separate from primary controller)
    // This provides network-based diagnostics and control regardless of primary controller type
    if (ctrl_cfg.driver_type != CONTROLLER_DRIVER_WIFI_TCP) {
        ESP_LOGI(TAG, "Initializing WiFi TCP controller as secondary RPC interface");
        controller_wifi_tcp_cfg_t wifi_rpc_cfg = {
            .listen_port = (uint16_t)wifi_ns_cfg->tcp_listen_port,
            .connection_timeout_ms = (uint16_t)wifi_ns_cfg->tcp_connection_timeout_ms,
        };
        controller_config_t wifi_ctrl_cfg = {
            .driver_type = CONTROLLER_DRIVER_WIFI_TCP,
            .task_stack = controller_cfg->task_stack, // Use same stack size as primary controller
            .task_prio = 8,  // Lower priority than primary controller
            .driver_cfg = &wifi_rpc_cfg,
            .driver_cfg_size = sizeof(wifi_rpc_cfg),
        };
        controller_driver_init_wifi_tcp(&wifi_ctrl_cfg);
    }
    
    // Run the 100 Hz locomotion loop in its own task pinned to APP_CPU (core 1).
    // Priority 14: above the controller driver (~8), below the RS485 bus master
    // (prio 15, also core 1 -- its per-leg read window is timing-critical) and
    // below the WiFi/BT stack on core 0. Previously this ran inline in the prio-1
    // main task, where every other subsystem preempted it -- the measured loop
    // rate was ~45 Hz with 40-90 ms period spikes and ~11 ms "busy" time.
    BaseType_t gait_ok = xTaskCreatePinnedToCore(
        gait_framework_main, "gait", 8192, NULL, 14, NULL, 1);
    if (gait_ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create gait task");
        return;
    }
    // app_main returns; the main task self-deletes and FreeRTOS keeps running.
}