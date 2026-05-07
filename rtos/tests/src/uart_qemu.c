/*
 * uart_qemu.c — Stellaris UART0 driver for QEMU lm3s6965evb
 *
 * UART0 base: 0x4000C000
 * DR (data register): offset 0x000
 * FR (flag register): offset 0x018, bit 5 = TXFF (TX FIFO full)
 */

#include <stdint.h>

#define UART0_DR  (*(volatile uint32_t *)0x4000C000)
#define UART0_FR  (*(volatile uint32_t *)0x4000C018)
#define UART0_TXFF (1 << 5)

void uart_putc(char c)
{
    while (UART0_FR & UART0_TXFF) {}
    UART0_DR = c;
}

void uart_print(const char *s)
{
    while (*s) {
        if (*s == '\n') uart_putc('\r');
        uart_putc(*s++);
    }
}

void print_int(int n)
{
    if (n < 0) { uart_putc('-'); n = -n; }
    if (n >= 10) print_int(n / 10);
    uart_putc('0' + (n % 10));
}
