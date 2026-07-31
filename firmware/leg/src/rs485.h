// RS485 half-duplex transport over RP2040 UART0.
//
// Uses the Pico SDK UART directly (not Arduino Serial1) so DE turnaround is
// fully under our control. arduino-pico has no hardware auto-DE; the ESP32
// master gets that free via UART_MODE_RS485_HALF_DUPLEX, but here we must drive
// the DE pin manually and, critically, wait for the UART to finish shifting the
// last byte (including its STOP bit) before releasing DE — otherwise the final
// CRC byte is truncated on the wire.

#ifndef HEX_LEG_RS485_H
#define HEX_LEG_RS485_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void rs485_init(void);

// Non-blocking receive. Accumulates incoming bytes into an internal line
// buffer. When a full '\n'-terminated frame has arrived, returns a pointer to
// the NUL-terminated line (without the '\n') and resets for the next frame.
// Returns NULL if no complete frame is available yet. The returned buffer is
// valid until the next call.
char *rs485_poll_line(void);

// Transmit a frame, managing DE: assert DE, write, wait for the shift register
// to drain, then release DE back to receive.
void rs485_send(const char *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif // HEX_LEG_RS485_H
