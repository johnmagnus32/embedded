// expect: 0
/* C99 __func__ (and GNU __FUNCTION__): a static string of the enclosing function's name. */
static int slen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static int streq(const char *a, const char *b) { for (int i = 0;; i++) { if (a[i] != b[i]) return 0; if (!a[i]) return 1; } }
static const char *who(void) { return __func__; }
int main(void) {
	if (!streq(__func__, "main")) return 1;
	if (!streq(who(), "who")) return 2;
	if (!streq(__FUNCTION__, "main")) return 3;
	if (slen(__func__) != 4) return 4;
	return 0;
}
