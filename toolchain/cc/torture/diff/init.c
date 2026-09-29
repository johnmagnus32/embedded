/* init.c — brace elision: string literals initializing char-array members (arrays of structs, nested structs, global
 * and local), pointer members, mixed members, partial initializers. Must match GCC. */
int printf(const char *, ...);
struct S { char w[8]; } gp[3] = { "abcdefg", "ABCDEFG", "zy" };
struct T { char *s; int n; } gt[2] = { "one", 1, "two", 2 };
struct U { struct S in; int k; char t[4]; } gu[2] = { "inner", 5, "abc", "x", 7 };
struct V { int a; char b[3]; char c[5]; } gv = { 1, "xy", "hello" };
static void dump(const char *tag, const void *p, int n) { const unsigned char *b = p; printf("%s:", tag); for (int i = 0; i < n; i++) printf(" %02x", b[i]); printf("\n"); }
int main(void) {
	struct S lp[3] = { "lmnopqr", "LM", "" };
	struct T lt[2] = { "l1", 3, "l2", 4 };
	struct U lu[2] = { "local", 6, "de", "yy" };
	struct V lv = { 2, "ab", "world" };
	dump("gp", gp, sizeof gp); dump("gu", gu, sizeof gu); dump("gv", &gv, sizeof gv);
	dump("lp", lp, sizeof lp); dump("lu", lu, sizeof lu); dump("lv", &lv, sizeof lv);
	printf("%s %d %s %d | %s %d %s %d\n", gt[0].s, gt[0].n, gt[1].s, gt[1].n, lt[0].s, lt[0].n, lt[1].s, lt[1].n);
	return 0;
}
