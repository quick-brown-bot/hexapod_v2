// Minimal USB-serial calibration / bring-up interface.
//
// Runs over the XIAO's USB CDC port (Arduino `Serial`), independent of the
// RS485 bus. Its primary job is setting the leg address before the board is
// mounted, since all six LegBoards run identical firmware. A fuller calibration
// tool (servo PWM ranges, current scale) is a planned follow-up; the command
// loop here is structured so those commands can be added later.

#ifndef HEX_LEG_CALIB_H
#define HEX_LEG_CALIB_H

#ifdef __cplusplus
extern "C" {
#endif

void calib_init(void);

// Non-blocking: process any pending USB-serial command bytes. Call from loop().
void calib_poll(void);

#ifdef __cplusplus
}
#endif

#endif // HEX_LEG_CALIB_H
