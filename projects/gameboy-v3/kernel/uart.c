/*
 * uart.c — board-independent console wrapper.
 *
 * The register-level UART driver lives in board_16550.c (T113) or board_pl011.c
 * (QEMU virt), selected by board.h. Each provides uart_hw_init/putc/getc; this
 * file adds the shared policy (NUL-free puts, \n -> \r\n translation) on top so
 * the two drivers stay minimal and identical in behavior.
 */

#include <stdint.h>
#include "uart.h"

/* provided by the selected board UART driver */
void uart_hw_init(void);
void uart_hw_putc(char c);
int  uart_hw_getc(void);

void uart0_init(void) { uart_hw_init(); }

void uart0_putc(char c) { uart_hw_putc(c); }

void uart0_puts(const char *s)
{
	for (; *s; s++) {
		if (*s == '\n')
			uart_hw_putc('\r');
		uart_hw_putc(*s);
	}
}

int uart0_getc(void) { return uart_hw_getc(); }
