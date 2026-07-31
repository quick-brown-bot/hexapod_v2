/*
 * Shared joint calibration type definitions.
 */

#ifndef HEXAPOD_JOINT_TYPES_H
#define HEXAPOD_JOINT_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float zero_offset_rad;
    int8_t invert_sign;
    float min_rad;
    float max_rad;
    int32_t pwm_min_us;
    int32_t pwm_max_us;
    int32_t neutral_us;
} joint_calib_t;

#ifdef __cplusplus
}
#endif

#endif // HEXAPOD_JOINT_TYPES_H
