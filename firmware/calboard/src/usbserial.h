// USB-serial command interface for the calibration adapter board. Mirrors
// the minimal ASCII line-protocol style of firmware/leg/src/calib.cpp.

#ifndef HEX_CALBOARD_USBSERIAL_H
#define HEX_CALBOARD_USBSERIAL_H

#ifdef __cplusplus
extern "C" {
#endif

void usbserial_init(void);

// Non-blocking: process any pending USB-serial command bytes. Call from
// loop(). Also owns applying PWM-decoded steps to the ladder each call for
// channels not under manual STEP override.
void usbserial_poll(void);

#ifdef __cplusplus
}
#endif

#endif // HEX_CALBOARD_USBSERIAL_H
