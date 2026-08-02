// Onboard status LED: blinks red at 2 Hz while the board is uncalibrated
// (leg_addr == 0, see persist.h/DEFAULT_LEG_ADDR), off once calibrated.

#ifndef HEX_LEG_STATUS_LED_H
#define HEX_LEG_STATUS_LED_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void status_led_init(void);

// Call every loop() iteration. Non-blocking; internally time-gated.
void status_led_update(bool calibrated, uint32_t now_us);

#ifdef __cplusplus
}
#endif

#endif // HEX_LEG_STATUS_LED_H
