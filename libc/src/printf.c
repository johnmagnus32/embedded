/*
 * printf.c — the printf/snprintf family over one format engine.
 *
 * A single vformat() parses the format and writes chunks to a `struct sink`, which is
 * EITHER a bounded buffer (snprintf/vsnprintf) OR an fd (printf/dprintf, unbuffered).
 * Directives: %s %c %d %i %u %x %X %o %p %% with field width, precision, the '-' (left)
 * and '0' (zero-fill) flags, '*' width/precision, and %.*s. `l`/`h`/`z` length modifiers
 * are parsed; `l`/`z` widen the argument to long.
 *
 * NOT yet supported: float (%f %e %g) — needs libm + decimal conversion; deferred to the
 * libm slice (see libc/PLAN.md). An unsupported directive is emitted literally and does
 * NOT consume a vararg (fine today since no in-tree caller uses float).
 *
 * The FILE* stream layer (fopen/fread/fprintf/...) lives in stdio.c and builds on vdprintf.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Output target: buf != NULL => append to buf (bounded by cap); else write() to fd.
 * `count` is the number of bytes that WOULD be produced (the snprintf/printf return). */
struct sink {
	char  *buf;
	size_t cap;
	size_t len;
	int    fd;
	int    count;
};

static void emit(struct sink *s, const char *data, int n)
{
	if (n <= 0) return;
	s->count += n;
	if (s->buf) {
		if (s->len < s->cap) {
			size_t room = s->cap - s->len;
			size_t c = (size_t)n < room ? (size_t)n : room;
			memcpy(s->buf + s->len, data, c);
			s->len += c;
		}
	} else {
		(void)write(s->fd, data, (size_t)n);
	}
}

static void emit_pad(struct sink *s, char c, int n)
{
	char blk[16];
	if (n <= 0) return;
	memset(blk, c, sizeof blk);
	while (n > 0) { int k = n < (int)sizeof blk ? n : (int)sizeof blk; emit(s, blk, k); n -= k; }
}

/* format one unsigned into buf in the given base; return the digit count. */
static int u2s(char *buf, unsigned long long v, unsigned base, int upper)
{
	char tmp[32];
	const char *digs = upper ? "0123456789ABCDEF" : "0123456789abcdef";
	int i = 0;
	do { tmp[i++] = digs[v % base]; v /= base; } while (v);
	int n = i;
	while (i) { buf[n - i] = tmp[i - 1]; i--; }
	return n;
}

/* Emit a field = [width pad] prefix [precision '0's] body, honoring '-'(left)/'0'(zero).
 * `zpad` is the precision zero-fill (digits); `zero` is the width '0' flag (mutually
 * exclusive with zpad, since a precision disables the '0' flag). */
static void emit_field(struct sink *s, const char *prefix, int plen,
                       const char *body, int blen, int zpad,
                       int width, int left, int zero)
{
	int content = plen + zpad + blen;
	int wpad = width > content ? width - content : 0;
	if (left) {
		emit(s, prefix, plen); emit_pad(s, '0', zpad); emit(s, body, blen); emit_pad(s, ' ', wpad);
	} else if (zero) {
		emit(s, prefix, plen); emit_pad(s, '0', wpad); emit_pad(s, '0', zpad); emit(s, body, blen);
	} else {
		emit_pad(s, ' ', wpad); emit(s, prefix, plen); emit_pad(s, '0', zpad); emit(s, body, blen);
	}
}

static int vformat(struct sink *s, const char *fmt, va_list ap)
{
	char nbuf[32];
	const char *p = fmt;
	while (*p) {
		if (*p != '%') {
			const char *start = p;
			while (*p && *p != '%') p++;
			emit(s, start, (int)(p - start));
			continue;
		}
		p++;                                   /* past '%' */

		int left = 0, zero = 0;
		for (;; p++) {
			if (*p == '-') left = 1;
			else if (*p == '0') zero = 1;
			else break;
		}
		int width = 0;
		if (*p == '*') { width = va_arg(ap, int); p++; if (width < 0) { left = 1; width = -width; } }
		else while (*p >= '0' && *p <= '9') width = width * 10 + (*p++ - '0');
		int prec = -1;
		if (*p == '.') {
			p++;
			if (*p == '*') { prec = va_arg(ap, int); p++; if (prec < 0) prec = -1; }
			else { prec = 0; while (*p >= '0' && *p <= '9') prec = prec * 10 + (*p++ - '0'); }
		}
		int lng = 0;
		while (*p == 'l') { lng++; p++; }
		while (*p == 'h') p++;
		if (*p == 'z') { lng = 1; p++; }

		char conv = *p;
		if (!conv) break;                       /* trailing '%' — stop cleanly */
		p++;

		switch (conv) {
		case 's': {
			const char *str = va_arg(ap, const char *);
			if (!str) str = "(null)";
			int sl;
			if (prec >= 0) { sl = 0; while (sl < prec && str[sl]) sl++; }   /* bounded: a precision means the arg need not be NUL-terminated */
			else sl = (int)strlen(str);
			emit_field(s, "", 0, str, sl, 0, width, left, 0);
			break;
		}
		case 'c': {
			char c = (char)va_arg(ap, int);
			emit_field(s, "", 0, &c, 1, 0, width, left, 0);
			break;
		}
		case 'd': case 'i': {
			long long v = (lng >= 2) ? va_arg(ap, long long) : lng ? va_arg(ap, long) : (long)va_arg(ap, int);
			int neg = v < 0;
			unsigned long long uv = neg ? (unsigned long long)(-(v + 1)) + 1ULL : (unsigned long long)v;
			int nn = u2s(nbuf, uv, 10, 0);
			if (prec == 0 && uv == 0) nn = 0;            /* zero value, precision 0 => no digits */
			int zpad = (prec > nn) ? prec - nn : 0;
			int zflag = (prec >= 0) ? 0 : zero;          /* a precision disables the '0' flag */
			emit_field(s, neg ? "-" : "", neg ? 1 : 0, nbuf, nn, zpad, width, left, zflag);
			break;
		}
		case 'u': case 'x': case 'X': case 'o': {
			unsigned long long uv = (lng >= 2) ? va_arg(ap, unsigned long long)
			                      : lng ? va_arg(ap, unsigned long) : (unsigned long long)va_arg(ap, unsigned);
			unsigned base = (conv == 'x' || conv == 'X') ? 16 : (conv == 'o' ? 8 : 10);
			int nn = u2s(nbuf, uv, base, conv == 'X');
			if (prec == 0 && uv == 0) nn = 0;            /* zero value, precision 0 => no digits */
			int zpad = (prec > nn) ? prec - nn : 0;
			int zflag = (prec >= 0) ? 0 : zero;
			emit_field(s, "", 0, nbuf, nn, zpad, width, left, zflag);
			break;
		}
		case 'p': {
			unsigned long v = (unsigned long)va_arg(ap, void *);
			int nn = u2s(nbuf, v, 16, 0);
			emit_field(s, "0x", 2, nbuf, nn, 0, width, left, zero);
			break;
		}
		case '%': {
			char pc = '%';
			emit_field(s, "", 0, &pc, 1, 0, width, left, 0);
			break;
		}
		default:                                 /* unsupported (incl float) — emit literally */
			emit(s, "%", 1);
			emit(s, &conv, 1);
			break;
		}
	}
	return s->count;
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
	struct sink s = { buf, size, 0, -1, 0 };
	int n = vformat(&s, fmt, ap);
	if (size) buf[s.len < size ? s.len : size - 1] = '\0';
	return n;
}

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list ap; va_start(ap, fmt);
	int n = vsnprintf(buf, size, fmt, ap);
	va_end(ap);
	return n;
}

/* sprintf — unbounded formatted write; the caller guarantees buf is large enough (C semantics).
 * Implemented over the bounded engine with a max bound. */
int sprintf(char *buf, const char *fmt, ...)
{
	va_list ap; va_start(ap, fmt);
	int n = vsnprintf(buf, (size_t)-1, fmt, ap);
	va_end(ap);
	return n;
}

int vdprintf(int fd, const char *fmt, va_list ap)
{
	struct sink s = { NULL, 0, 0, fd, 0 };
	return vformat(&s, fmt, ap);
}

int dprintf(int fd, const char *fmt, ...)
{
	va_list ap; va_start(ap, fmt);
	int n = vdprintf(fd, fmt, ap);
	va_end(ap);
	return n;
}

int vprintf(const char *fmt, va_list ap) { return vdprintf(1, fmt, ap); }

int printf(const char *fmt, ...)
{
	va_list ap; va_start(ap, fmt);
	int n = vdprintf(1, fmt, ap);
	va_end(ap);
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
