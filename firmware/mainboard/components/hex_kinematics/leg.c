/*
 * Pure IK implementation: compute joint angles from foot position.
 */

#include "leg.h"
#include <stdlib.h>
#include <math.h>
#include "esp_err.h"
#include "esp_log.h"

static const char* TAG = "leg";

static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline float normalize_angle(float angle)
{
    while (angle > M_PI) angle -= 2.0f * M_PI;
    while (angle < -M_PI) angle += 2.0f * M_PI;
    return angle;
}

esp_err_t leg_configure(const leg_geometry_t* cfg, leg_handle_t* out_leg)
{
    if (!cfg || !out_leg) return ESP_ERR_INVALID_ARG;
    *out_leg = NULL;
    leg_geometry_t* leg = (leg_geometry_t*)calloc(1, sizeof(leg_geometry_t));
    if (!leg) return ESP_ERR_NO_MEM;
    
    // Copy configuration to allocated structure
    *leg = *cfg;
    *out_leg = leg;
    return ESP_OK;
}

esp_err_t leg_ik_solve(leg_handle_t handle, float x, float y, float z, leg_angles_t *out_angles)
{
    /*
     * Right-handed leg-local frame (UPDATED):
     *   X: outward from body mounting point (+)
     *   Y: forward (+)
     *   Z: up (+)
     * Input (x,y,z) is the desired foot position in this frame.
     * Previous implementation assumed Z down; do NOT reuse old sign assumptions.
     */
    if (!handle || !out_angles) return ESP_ERR_INVALID_ARG;
    leg_geometry_t* leg = handle;

    // Coxa yaw in XY plane. No offset needed: yaw=0 already means "straight
    // out along +X" for any leg, which is the natural, unambiguous zero.
    float yaw = normalize_angle(atan2f(y, x));
    float r_xy = sqrtf(x * x + y * y);
    ESP_LOGD(TAG, "IK(Zup): r_xy=%.3f, yaw=%.3f deg", r_xy, yaw * (180.0f / M_PI));

    // Project to sagittal plane after removing coxa length along X (outward)
    float px = r_xy - leg->len_coxa;   // horizontal distance from femur joint
    float pz = z;                      // height (Z up)

    // Dist from femur joint to foot projection in XZ plane (using Z up now)
    float L1 = leg->len_femur;
    float L2 = leg->len_tibia;
    float d = clampf(hypotf(px, pz), fabsf(L1 - L2), L1 + L2);
    ESP_LOGD(TAG, "IK(Zup): px=%.3f pz=%.3f d=%.3f", px, pz, d);

    // Knee (tibia) via law of cosines: interior angle at the knee between femur
    // and tibia. 0 when fully folded (femur and tibia doubled back onto each
    // other), pi (180 deg) when fully extended (straight leg).
    float cosK = (L1*L1 + L2*L2 - d*d) / (2.0f * L1 * L2);
    cosK = clampf(cosK, -1.0f, 1.0f);
    float knee_angle = acosf(cosK);
    // tibia_offset_rad shifts this to whatever this leg's build calls zero --
    // e.g. pi/2 for "tibia straight down from the knee, foot under the knee
    // joint" (a right angle at the knee is the geometric definition of that
    // pose, for any link lengths). See docs/architecture/HARDWARE_AND_MECHANICS.md
    // "Joint Angle Sign Convention". Increasing tibia bends the leg downward
    // (foot lowers) in this Z-up space.
    float tibia = knee_angle - leg->tibia_offset_rad;
    ESP_LOGD(TAG, "IK(Zup): cosK=%.3f knee=%.3f deg tibia=%.3f deg", cosK, knee_angle * (180.0f / M_PI), tibia * (180.0f / M_PI));

    // Femur (hip pitch): angle of the hip-to-target line above horizontal
    // (base), plus the interior hip angle between the femur link and that
    // line (phi). No offset needed: at the neutral pose (femur horizontal,
    // tibia straight down, foot under the knee) base and phi are equal
    // magnitude and opposite sign for any link lengths, so they cancel to
    // exactly zero -- confirmed both algebraically and on real hardware.
    float cosPhi = (L1*L1 + d*d - L2*L2) / (2.0f * L1 * d);
    cosPhi = clampf(cosPhi, -1.0f, 1.0f);
    float phi = acosf(cosPhi); // interior angle at hip between femur and line to foot
    float base = atan2f(pz, px); // angle of foot line above horizontal
    float femur = base + phi;
    ESP_LOGD(TAG, "IK(Zup): cosPhi=%.3f phi=%.3f deg base=%.3f deg femur=%.3f deg", cosPhi, phi * (180.0f / M_PI), base * (180.0f / M_PI), femur * (180.0f / M_PI));

    out_angles->coxa = yaw;
    out_angles->femur = femur;
    out_angles->tibia = tibia;
    return ESP_OK;
}
