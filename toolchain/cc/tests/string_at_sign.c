// expect: 0
/* '@' (ARM asm comment char), '//' and '/*' INSIDE a string literal are string bytes, not comments (kernel
 * format strings like "initcall %pS @ %i"). Was: our as truncated the string at '@' and blanked the rest. */
static int streq(const char *a, const char *b) { for (int i = 0;; i++) { if (a[i] != b[i]) return 0; if (!a[i]) return 1; } }
static int slen(const char *s) { int n = 0; while (s[n]) n++; return n; }
int main(void) {
	const char *s = "call %pS @ %i // not /* a */ comment";
	if (slen(s) != 36) return 1;
	if (!streq(s + 9, "@ %i // not /* a */ comment")) return 2;
	if (slen("q\"@x") != 4) return 3;
	return 0;
}
