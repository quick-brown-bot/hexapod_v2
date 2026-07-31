/*
 * Public joint calibration namespace types and APIs.
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "types/joint_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NUM_LEGS 6
#define NUM_JOINTS_PER_LEG 3

typedef struct {
    joint_calib_t joints[NUM_LEGS][NUM_JOINTS_PER_LEG];
} joint_calib_config_t;

const joint_calib_config_t* config_get_joint_calib(void);
esp_err_t config_set_joint_calib(const joint_calib_config_t* config);

esp_err_t config_get_joint_calib_data(int leg_index, int joint, joint_calib_t* calib_data);
esp_err_t config_set_joint_calib_data_memory(int leg_index, int joint, const joint_calib_t* calib_data);
esp_err_t config_set_joint_calib_data_persist(int leg_index, int joint, const joint_calib_t* calib_data);

esp_err_t config_set_joint_offset_memory(int leg_index, int joint, float offset_rad);
esp_err_t config_set_joint_offset_persist(int leg_index, int joint, float offset_rad);

esp_err_t config_set_joint_limits_memory(int leg_index, int joint, float min_rad, float max_rad);
esp_err_t config_set_joint_limits_persist(int leg_index, int joint, float min_rad, float max_rad);

esp_err_t config_set_joint_pwm_memory(int leg_index, int joint, int32_t pwm_min_us, int32_t pwm_max_us, int32_t pwm_neutral_us);
esp_err_t config_set_joint_pwm_persist(int leg_index, int joint, int32_t pwm_min_us, int32_t pwm_max_us, int32_t pwm_neutral_us);

void config_load_joint_calib_defaults(joint_calib_config_t* config);

#ifdef __cplusplus
}
#endif
