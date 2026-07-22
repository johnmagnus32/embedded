/*
 * dram_shim.c — the one non-inline shim the DRAM driver needs: a tiny printf.
 * Supports just %d %u %x %s %c %% (enough for dram.c's status messages, e.g.
 * "ZQ calibration error, check external 240 ohm resistor" and "DRAM: size = %dMB").
 * Routes to UART0.
 */

#include <stdint.h>
#include <stdarg.h>
#include "uart.h"

/* Busy-loop delays (declared in dram_shim.h). ~200 nop/us is a rough guess for
 * the A7 at boot clock; callers only need a lower bound, so overshoot is safe. */
void udelay(unsigned long us)
{
	volatile unsigned long n = us * 200UL;
	while (n--)
		__asm__ volatile("nop");
}
void mdelay(unsigned long ms) { udelay(ms * 1000UL); }

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
	int i;
	for (i = 28; i >= 0; i -= 4)
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
		fmt++;                       /* consume '%' */
		switch (*fmt) {
		case 'd': {
			int v = va_arg(ap, int);
			if (v < 0) { uart0_putc('-'); v = -v; }
			put_udec((unsigned int)v);
			break;
		}
		case 'u': put_udec(va_arg(ap, unsigned int)); break;
		case 'x': put_hex(va_arg(ap, unsigned int)); break;
		case 's': {
			const char *s = va_arg(ap, const char *);
			uart0_puts(s ? s : "(null)");
			break;
		}
		case 'c': uart0_putc((char)va_arg(ap, int)); break;
		case '%': uart0_putc('%'); break;
		default:  uart0_putc('%'); uart0_putc(*fmt); break;
		}
	}
	va_end(ap);
	return 0;
}
