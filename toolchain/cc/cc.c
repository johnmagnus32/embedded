/*
 * cc.c — the driver: read a C source file, run lex -> parse -> gen, and write the ARM assembly (.s) that
 * our as then turns into an object. Deliberately does NOT emit ELF itself — like a real cc it stops at
 * assembly text and leaves object/exe production to as + ld. One translation unit at a time.
 *
 * Usage: cc [-o out.s] in.c        (defaults to a.s)
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "cc.h"

void die(const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	fputs("cc: ", stderr); vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap); exit(1);
}

int main(int argc, char **argv) {
	const char *out = "a.s", *in = NULL;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
		else if (argv[i][0] != '-') in = argv[i];
		else die("unknown option '%s'", argv[i]);
	}
	if (!in) die("usage: cc [-o out.s] in.c");

	FILE *f = fopen(in, "rb"); if (!f) die("cannot open %s", in);
	fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
	char *src = malloc(sz + 1); if (fread(src, 1, sz, f) != (size_t)sz) die("read failed"); src[sz] = 0; fclose(f);

	Token *tok = lex(src);
	Func *prog = parse(tok);
	gen(prog, out);
	return 0;
}
