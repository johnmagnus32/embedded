/*
 * printf.c — tiny printf for the kernel console (routes to UART0).
 * Supports %d %u %x %s %c %%. Same minimal implementation as the bootloader.
 */

#include <stdint.h>
#include <stdarg.h>
#include "uart.h"

static void put_udec(unsigned int v)
{
	char buf[10];
	int i = 0;
	if (v == 0) { uart0_putc('0'); return; }
	while (v) { buf[i++] = '0' + (v % 10); v /= 10; }
	while (i--) uart0_putc(buf[i]);
}

static void put_hex(unsigned int v)
{
	const char *hex = "0123456789abcdef";
	for (int i = 28; i >= 0; i -= 4)
		uart0_putc(hex[(v >> i) & 0xf]);
}

int printf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	for (; *fmt; fmt++) {
		if (*fmt != '%') {
			if (*fmt == '\n') uart0_putc('\r');
			uart0_putc(*fmt);
			continue;
		}
		fmt++;
		switch (*fmt) {
		case 'd': {
			int v = va_arg(ap, int);
			if (v < 0) { uart0_putc('-'); v = -v; }
			put_udec((unsigned)v);
			break;
		}
		case 'u': put_udec(va_arg(ap, unsigned)); break;
		case 'x': put_hex(va_arg(ap, unsigned)); break;
		case 's': { const char *s = va_arg(ap, const char *); uart0_puts(s ? s : "(null)"); break; }
		case 'c': uart0_putc((char)va_arg(ap, int)); break;
		case '%': uart0_putc('%'); break;
		default:  uart0_putc('%'); uart0_putc(*fmt); break;
		}
	}
	va_end(ap);
	return 0;
}
