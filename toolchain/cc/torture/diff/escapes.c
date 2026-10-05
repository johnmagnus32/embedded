/* escape sequences, including GNU's \e / \E (ESC) — in strings, wide strings and character constants */
#include <stdio.h>

int main(void) {
	const char *s = "\e[?25h\E\a\b\f\v\x1b\033\?\'\"\\";
	for (const char *p = s; *p; p++) printf("%02x ", (unsigned char)*p);
	printf("| %d %d %d\n", '\e', '\E', (int)sizeof "\e");
	const __WCHAR_TYPE__ *w = L"\e\x263a";
	printf("%x %x\n", (unsigned)w[0], (unsigned)w[1]);
	return 0;
}
