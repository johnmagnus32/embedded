/*
 * printf_diff.c — differential test of the libc format engine against glibc.
 *
 * The custom engine (libc/src/stdio.c) is compiled on the host with its public symbols
 * renamed (cust_snprintf, see run.sh), so this file can pull in glibc's <stdio.h> as the
 * ORACLE. For each case we format the SAME (fmt, args) with both and compare the result
 * string AND the return value byte-for-byte. Any divergence is a bug in the custom engine.
 *
 * Scope matches slice 1: integer/string/char/pointer directives with width, precision,
 * '-'/'0' flags, '*' width/precision and %.*s. Float (%f/%e/%g) is intentionally NOT
 * exercised (deferred to the libm+stdio slice). %p with NULL is skipped (glibc prints
 * "(nil)", a quirk the custom engine does not mimic).
 */
#include <stdio.h>
#include <string.h>
#include <limits.h>

extern int cust_snprintf(char *buf, size_t size, const char *fmt, ...);

static int ran = 0, fails = 0;

#define CK(fmt, ...) do {                                                          \
	char a[256], b[256];                                                           \
	int na = snprintf(a, sizeof a, fmt, ##__VA_ARGS__);                            \
	int nb = cust_snprintf(b, sizeof b, fmt, ##__VA_ARGS__);                        \
	ran++;                                                                         \
	if (strcmp(a, b) != 0 || na != nb) {                                           \
		fails++;                                                                   \
		printf("  MISMATCH  fmt=\"%s\"  glibc=[%s](%d)  cust=[%s](%d)\n",          \
		       fmt, a, na, b, nb);                                                 \
	}                                                                              \
} while (0)

int main(void)
{
	/* plain + integers */
	CK("hello");
	CK("%d", 0);
	CK("%d", 42);
	CK("%d", -42);
	CK("%d", INT_MAX);
	CK("%d", INT_MIN);
	CK("%i", -7);
	CK("%u", 0u);
	CK("%u", 4000000000u);
	CK("%x", 0xdeadbeefu);
	CK("%X", 0xdeadbeefu);
	CK("%o", 0755u);

	/* width */
	CK("[%5d]", 42);
	CK("[%5d]", -42);
	CK("[%-5d]", 42);
	CK("[%05d]", 42);
	CK("[%05d]", -42);
	CK("[%8x]", 0xabcu);
	CK("[%-8x]", 0xabcu);
	CK("[%08x]", 0xabcu);

	/* precision (integers = minimum digits) */
	CK("[%.4d]", 7);
	CK("[%.0d]", 0);
	CK("[%.4d]", -7);
	CK("[%8.4d]", 7);
	CK("[%-8.4d]", 7);
	CK("[%08.4d]", 7);      /* precision disables the '0' flag -> space pad */

	/* strings */
	CK("%s", "hello");
	CK("%s", "");
	CK("[%8s]", "hi");
	CK("[%-8s]", "hi");
	CK("[%.3s]", "hello");
	CK("[%.0s]", "hello");
	CK("[%8.3s]", "hello");
	CK("[%-8.3s]", "hello");
	CK("%s", (char *)0);   /* both should render "(null)" */

	/* char + literal percent */
	CK("%c", 'A');
	CK("[%3c]", 'A');
	CK("[%-3c]", 'A');
	CK("100%%");

	/* '*' width / precision */
	CK("[%*d]", 6, 42);
	CK("[%-*d]", 6, 42);
	CK("[%.*d]", 4, 7);
	CK("[%.*s]", 3, "hello");
	CK("[%*.*s]", 8, 3, "hello");
	CK("[%*d]", -6, 42);   /* negative width => left-justify */

	/* pointer (non-NULL, fixed value so glibc == cust) */
	CK("%p", (void *)0x1234);
	CK("[%12p]", (void *)0xabcdef);

	/* long long (the 'll' modifier must read a 64-bit vararg, not a 32-bit long) */
	CK("%lld", -5000000000LL);
	CK("%llu", 18000000000ULL);
	CK("%llx", 0x123456789ULL);
	CK("[%12lld]", 42LL);

	/* precision-bounded %s over a NON-NUL-terminated buffer: must read at most `prec` bytes
	 * (ASan in run.sh traps an over-read; glibc is the safe oracle). */
	{ char nt[3] = { 'x', 'y', 'z' };
	  CK("[%.3s]", nt);
	  CK("[%.2s]", nt);
	  CK("[%.*s]", 3, nt); }

	/* mixed */
	CK("%s=%d, %s=%u, hex=%x", "a", 1, "b", 2u, 255u);
	CK("[%-10s|%5d|%04x]", "tag", 7, 0x2a);

	printf("printf_diff: %d/%d cases matched\n", ran - fails, ran);
	if (fails == 0) printf("PRINTF_DIFF_OK\n");
	return fails ? 1 : 0;
}
