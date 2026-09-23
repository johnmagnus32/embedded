// expect: 42
// Long string literals (asm templates / format strings) exceeded Token.text[64] and truncated (kernel
// ip_fast_csum's ~374-char inline-asm template became "su"). Now stored via a malloc'd sval, unbounded.
static int sl(const char *s) { int n = 0; while (*s++) n++; return n; }
int main(void)
{
	const char *m = "this string literal is intentionally far longer than sixty-four bytes to exercise sval";
	int a = 7;
	__asm__("add %0, %0, #5\n\tadd %0, %0, #100\n\tsub %0, %0, #100\n\tadd %0, %0, #30" : "+r" (a));
	return (sl(m) > 64) ? a : 1;   /* template: 7+5+100-100+30 = 42; string must be untruncated */
}
