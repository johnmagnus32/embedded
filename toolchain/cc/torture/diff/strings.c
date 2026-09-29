/* strings.c — string literals: adjacent pieces decoded separately ("\x41" "B", "\1" "2"), embedded NULs, every byte
 * value, quotes/backslashes, wide concatenation (L"" with narrow pieces, u"", U""), char[] / wchar_t[] initializers
 * and their sizes. Must match GCC. */
int printf(const char *, ...);
typedef unsigned int wchar;
static const char a[] = "\x41" "B" "\1" "2" "\0" "z" "\"\\" "\377\200";
static const wchar w[] = L"\x41" "B" L"é" "\x7f";
static const unsigned short u[] = u"x" "\x62" u"☃";
static void dump(const char *t, const unsigned char *p, int n) { printf("%s %d:", t, n); for (int i = 0; i < n; i++) printf(" %02x", p[i]); printf("\n"); }
int main(void) {
	const char *s = "\x41" "B" "\1" "2" "\0" "z";
	unsigned char all[256]; for (int i = 0; i < 256; i++) all[i] = (unsigned char)i;
	const char *every = "\000\001\002\003\004\005\006\007\010\011\012\013\014\015\016\017\020\021\022\023\024\025\026\027\030\031\032\033\034\035\036\037"
		" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\177"
		"\200\201\202\203\204\205\206\207\210\211\212\213\214\215\216\217\220\221\222\223\224\225\226\227\230\231\232\233\234\235\236\237"
		"\240\241\242\243\244\245\246\247\250\251\252\253\254\255\256\257\260\261\262\263\264\265\266\267\270\271\272\273\274\275\276\277"
		"\300\301\302\303\304\305\306\307\310\311\312\313\314\315\316\317\320\321\322\323\324\325\326\327\330\331\332\333\334\335\336\337"
		"\340\341\342\343\344\345\346\347\350\351\352\353\354\355\356\357\360\361\362\363\364\365\366\367\370\371\372\373\374\375\376\377";
	int same = 1; for (int i = 0; i < 256; i++) same &= (unsigned char)every[i] == all[i];
	dump("a", (const unsigned char *)a, sizeof a); dump("s", (const unsigned char *)s, 7);
	dump("w", (const unsigned char *)w, sizeof w); dump("u", (const unsigned char *)u, sizeof u);
	printf("every %d size %d %d\n", same, (int)sizeof "\x41" "B", (int)sizeof L"a" "b");
	return 0;
}
