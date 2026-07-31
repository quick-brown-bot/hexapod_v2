#include "rs485.h"
#include "config.h"

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"

#define RS485_LINE_MAX 128

static uart_inst_t *const s_uart = uart0; // TX=GP0, RX=GP1

static char   s_line[RS485_LINE_MAX];
static size_t s_len = 0;

void rs485_init(void)
{
    uart_init(s_uart, RS485_BAUD);
    gpio_set_function(PIN_UART_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_UART_RX, GPIO_FUNC_UART);
    uart_set_format(s_uart, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(s_uart, true);
    uart_set_translate_crlf(s_uart, false);

    // DE pin: drive low (receive) by default.
    gpio_init(PIN_RS485_DE);
    gpio_set_dir(PIN_RS485_DE, GPIO_OUT);
    gpio_put(PIN_RS485_DE, 0);

    s_len = 0;
}

char *rs485_poll_line(void)
{
    while (uart_is_readable(s_uart)) {
        char c = (char)uart_getc(s_uart);
        if (c == '\n') {
            s_line[s_len] = '\0';
            size_t n = s_len;
            s_len = 0;
            (void)n;
            return s_line;
        }
        if (c == '\r') {
            continue; // ignore CR
        }
        if (s_len + 1 < RS485_LINE_MAX) {
            s_line[s_len++] = c;
        } else {
            // Overflow: drop the partial frame and resync on the next '\n'.
            s_len = 0;
        }
    }
    return 0;
}

void rs485_send(const char *buf, size_t len)
{
    gpio_put(PIN_RS485_DE, 1);           // drive the bus
    uart_write_blocking(s_uart, (const uint8_t *)buf, len);
    uart_tx_wait_blocking(s_uart);       // FIFO + shift register drained...
    // ...but uart_tx_wait_blocking can return while the final STOP bit is still
    // on the wire. Poll the BUSY flag to be certain before releasing DE.
    while (uart_get_hw(s_uart)->fr & UART_UARTFR_BUSY_BITS) {
        tight_loop_contents();
    }
    gpio_put(PIN_RS485_DE, 0);           // back to receive

    // The DE/RE turnaround glitch can leave a spurious byte (e.g. a stray echo
    // of our own trailing stop bit) in the RX FIFO right as the receiver
    // re-enables. Left alone, that byte is picked up as a bogus empty line by
    // rs485_poll_line() on the next call, eating one real pull frame's framing
    // and causing the master to see an every-other-request timeout. Drain it.
    while (uart_is_readable(s_uart)) {
        (void)uart_getc(s_uart);
    }
}
