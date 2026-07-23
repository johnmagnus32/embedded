/*
 * stdio.c — a tiny unbuffered printf family over write(2). Enough for our
 * coreutils + shell. Formats: %s %c %d %i %u %x %p %%, with a minimal field
 * width via '0'/digit padding NOT supported yet (kept deliberately small; add
 * width/precision when a program actually needs it). Each conversion is emitted
 * as it's parsed; output goes to the target fd in modest chunks.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* format one unsigned into `buf` in the given base; return length. */
static int u2s(char *buf, unsigned long v, unsigned base, int upper)
{
	char tmp[32];
	const char *digs = upper ? "0123456789ABCDEF" : "0123456789abcdef";
	int i = 0;
	do { tmp[i++] = digs[v % base]; v /= base; } while (v);
	int n = i;
	while (i) buf[n - i] = tmp[i - 1], i--;
	return n;
}

static int vprint(int fd, const char *fmt, __builtin_va_list ap)
{
	char out[256];
	int total = 0;

	/* flush helper: write the accumulated `out[0..len)` */
	#define FLUSH(len) do { if (len) { write(fd, out, (len)); total += (len); } } while (0)

	int len = 0;
	for (const char *p = fmt; *p; p++) {
		if (*p != '%') {
			out[len++] = *p;
			if (len == sizeof(out)) { FLUSH(len); len = 0; }
			continue;
		}
		FLUSH(len); len = 0;          /* emit literal run before the conversion */
		p++;                          /* skip '%' */
		char nbuf[32]; int nn;
		switch (*p) {
		case 's': {
			const char *s = __builtin_va_arg(ap, const char *);
			if (!s) s = "(null)";
			int sl = (int)strlen(s);
			write(fd, s, sl); total += sl;
			break;
		}
		case 'c': {
			char c = (char)__builtin_va_arg(ap, int);
			write(fd, &c, 1); total += 1;
			break;
		}
		case 'd': case 'i': {
			long v = __builtin_va_arg(ap, int);
			if (v < 0) { write(fd, "-", 1); total += 1; v = -v; }
			nn = u2s(nbuf, (unsigned long)v, 10, 0);
			write(fd, nbuf, nn); total += nn;
			break;
		}
		case 'u': nn = u2s(nbuf, (unsigned long)__builtin_va_arg(ap, unsigned), 10, 0); write(fd, nbuf, nn); total += nn; break;
		case 'x': nn = u2s(nbuf, (unsigned long)__builtin_va_arg(ap, unsigned), 16, 0); write(fd, nbuf, nn); total += nn; break;
		case 'X': nn = u2s(nbuf, (unsigned long)__builtin_va_arg(ap, unsigned), 16, 1); write(fd, nbuf, nn); total += nn; break;
		case 'p': {
			unsigned long v = (unsigned long)__builtin_va_arg(ap, void *);
			write(fd, "0x", 2); total += 2;
			nn = u2s(nbuf, v, 16, 0); write(fd, nbuf, nn); total += nn;
			break;
		}
		case '%': write(fd, "%", 1); total += 1; break;
		case '\0': p--; break;        /* trailing '%' — stop cleanly */
		default:  write(fd, p, 1); total += 1; break;   /* unknown: emit literally */
		}
	}
	FLUSH(len);
	#undef FLUSH
	return total;
}

int dprintf(int fd, const char *fmt, ...)
{
	__builtin_va_list ap; __builtin_va_start(ap, fmt);
	int n = vprint(fd, fmt, ap);
	__builtin_va_end(ap);
	return n;
}

int printf(const char *fmt, ...)
{
	__builtin_va_list ap; __builtin_va_start(ap, fmt);
	int n = vprint(1, fmt, ap);
	__builtin_va_end(ap);
	return n;
}

int puts(const char *s)
{
	int n = (int)strlen(s);
	write(1, s, n);
	write(1, "\n", 1);
	return n + 1;
}

int putchar(int c)
{
	char ch = (char)c;
	write(1, &ch, 1);
	return c;
}
