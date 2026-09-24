// expect: 0
/* `char arr[] = "..."` must emit the string BYTES inline (indexable), not a pointer to an anonymous
 * string (the `char *p = "..."` case). Regression for base64_table[idx] reading garbage. Covers unsized
 * sizing, sized zero-fill, exact-fit (no NUL), escape decoding, and a RUNTIME index (the actual failure). */

static const char t[] = "TWFu";          /* unsized -> 5 bytes incl NUL */
static char s8[8] = "hi";                 /* sized -> h,i,0,0,0,0,0,0 */
static char ex[3] = "abc";                /* exact fit -> a,b,c (no NUL) */
static const char esc[] = "A\tB\x41\0Z";  /* escapes + embedded NUL -> A,0x09,B,0x41,0x00,Z,NUL = 7 */

static int rt(int i) { return i; }        /* defeat constant-folding of the index */

int main(void) {
	int i = rt(2);
	if (t[0] != 'T') return 1;
	if (t[i] != 'F') return 2;             /* runtime index into an inline char array */
	if (t[3] != 'u') return 3;
	if (t[4] != 0) return 4;
	if (sizeof(t) != 5) return 5;
	if (s8[0] != 'h' || s8[1] != 'i' || s8[2] != 0 || s8[7] != 0) return 6;
	if (sizeof(s8) != 8) return 7;
	if (ex[0] != 'a' || ex[1] != 'b' || ex[2] != 'c') return 8;
	if (sizeof(ex) != 3) return 9;
	if (esc[0] != 'A' || esc[1] != '\t' || esc[2] != 'B') return 10;
	if (esc[3] != 0x41 || esc[4] != 0 || esc[5] != 'Z') return 11;
	return 0;
}
