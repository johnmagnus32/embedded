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
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>
#include "cc.h"

/* DIAGNOSTICS WITH A PLACE. Without cpp's line markers every message named a line in the preprocessed
 * temporary the driver then deleted, and no file: a missing semicolon in widget.h:3 came out as "(line 6)".
 * These print GCC's shape — the include chain, then `file:line: error: ...` — from the position recorded on
 * the token. Locations are file:line only: a column would need the original line's text, which we do not keep,
 * and GCC's columns count tab stops rather than bytes, so a wrong column is worse than none. */
static void chain(void) {   /* GCC's `In file included from a.c:3,\n                 from b.h:7:` */
	for (int i = 0; i < lex_inc_depth(); i++) {
		const char *f = lex_inc_file(i); if (!f) continue;
		fprintf(stderr, "%s %s:%d%s\n", i ? "                 from" : "In file included from",
		        f, lex_inc_line(i), i + 1 < lex_inc_depth() ? "," : ":");
	}
}
static void where(const char *file, int line) {
	if (file) { chain(); fprintf(stderr, "%s:%d: ", file, line); }
	else fputs("cc: ", stderr);
}
void warn_atl(const char *file, int line, const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	where(file, line); fputs("warning: ", stderr); vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap);
}
void die_atl(const char *file, int line, const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	where(file, line); fputs("error: ", stderr); vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap); exit(1);
}
void warn(const char *fmt, ...) {   /* no position available (driver-level, or input with no line markers) */
	va_list ap; va_start(ap, fmt);
	fputs("cc: warning: ", stderr); vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap);
}
void die(const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	fputs("cc: ", stderr); vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap); exit(1);
}

/* The diagnostics, and their defaults. gcc 13 WARNS on an implicit declaration rather than erroring — old
 * DJB-style C like runit's still relies on that — and the rest here are on by default in gcc too (they are
 * -Wall-level or unconditional), so "on, not fatal" is the honest default for each. The kernel passes
 * -Werror= for all seven and gets exactly what it asks for. */
unsigned char diag_sev[D_COUNT] = {
	[D_IMPLICIT_FN] = SEV_WARN, [D_IMPLICIT_INT] = SEV_WARN, [D_STRICT_PROTOTYPES] = SEV_OFF,
	[D_INCOMPATIBLE_PTR] = SEV_WARN, [D_RETURN_TYPE] = SEV_WARN, [D_DESIGNATED_INIT] = SEV_WARN,
	[D_DATE_TIME] = SEV_OFF,
};
/* -Wstrict-prototypes and -Wdate-time are OFF by default, as in gcc: K&R declarations and __DATE__ are legal
 * C that only a project with a policy about them wants to hear about. */
static const char *const diag_names[D_COUNT] = {
	[D_IMPLICIT_FN] = "implicit-function-declaration", [D_IMPLICIT_INT] = "implicit-int",
	[D_STRICT_PROTOTYPES] = "strict-prototypes", [D_INCOMPATIBLE_PTR] = "incompatible-pointer-types",
	[D_RETURN_TYPE] = "return-type", [D_DESIGNATED_INIT] = "designated-init", [D_DATE_TIME] = "date-time",
};
/* -w. A LATCH, not an ordered change: measured against gcc, `-w -Wimplicit-int` and `-Wimplicit-int -w` are
 * both silent, so it cannot be implemented by clearing severities as the flags are read. */
int diag_inhibit;
const char *diag_name(int d) { return d >= 0 && d < D_COUNT ? diag_names[d] : "?"; }
int diag_set(const char *name, int sev) {
	for (int d = 0; d < D_COUNT; d++)
		if (!strcmp(name, diag_names[d])) { diag_sev[d] = (unsigned char)sev; return 1; }
	if (!strcmp(name, "implicit")) {   /* gcc's umbrella for the two implicit-* ones */
		diag_sev[D_IMPLICIT_FN] = diag_sev[D_IMPLICIT_INT] = (unsigned char)sev; return 1;
	}
	return 0;
}
void diag_all(int sev) {                /* -Werror: promote every diagnostic that is ON; the off ones stay off */
	for (int d = 0; d < D_COUNT; d++) if (diag_sev[d] != SEV_OFF) diag_sev[d] = (unsigned char)sev;
}
int pic = 0;   /* -fPIC: position-independent code — a preemptible symbol through the GOT, the rest PC-relative */
int word_relocs = 0;   /* -mword-relocations: an address is a .word (R_ARM_ABS32), never movw/movt halves */
int default_vis = 0;   /* -fvisibility= */
const char *vis_directive(int vis) { return vis == VIS_HIDDEN ? ".hidden" : vis == VIS_INTERNAL ? ".internal" : vis == VIS_PROTECTED ? ".protected" : NULL; }
int vis_local(int vis) { return vis == VIS_HIDDEN || vis == VIS_INTERNAL; }   /* invisible outside this module, so nothing can interpose it */
int soft_float = 0;   /* -mfloat-abi=soft (the kernel): no VFP code or ABI; default hard (gnueabihf) */
int general_regs_only = 0;   /* -mgeneral-regs-only: the FP registers are off limits, but the ABI stays hard-float
                              * (gcc keeps Tag_ABI_VFP_args) — our own kernel builds with this */
/* What GCC records in the object's build attributes (and our lexer's wchar_t width): the target options. */
int unwind_tables = 0;    /* -funwind-tables: an EHABI unwind entry per function (.fnstart/.save/.setfp/.fnend) */
int short_wchar = 0;      /* -fshort-wchar: wchar_t is 16-bit (L"" / L'' are halfwords; Tag_ABI_PCS_wchar_t 2) */
unsigned fixed_regs;   /* -ffixed-REG: registers the code generator must leave alone (U-Boot: r9) */
int function_sections = 0, data_sections = 0;   /* -ffunction-sections / -fdata-sections: one section per function / object
                                                 * (.text.f, .data.x, ...: GCC's names), so a --gc-sections link can drop each */
const char *opt_march;    /* -march=armv7-a (NULL: -mcpu=cortex-a7, the target's own) */
const char *opt_mfpu = "neon-vfpv4";   /* -mfpu= */
char opt_olevel = '0';    /* -O level (Tag_ABI_optimization_goals) */
int no_unaligned, unsafe_math, finite_math, softfp_abi;
int gnu89_inline = 0;   /* -fgnu89-inline (and the gnu89/c89 dialects): `inline` is an external definition, `extern
                         * inline` the inline-only one — the C99 rule is the other way round */

/* Recursive descent follows the program's nesting, and 10000 nested struct definitions or declarators are legal C:
 * the compiler runs on a thread with a 256 MiB stack (address space reserved; pages touched only as used). */
static int g_argc; static char **g_argv;
static void *compile(void *unused);
int main(int argc, char **argv) {
	g_argc = argc; g_argv = argv;
	pthread_attr_t a; pthread_t t;
	if (pthread_attr_init(&a) || pthread_attr_setstacksize(&a, (size_t)256 << 20) || pthread_create(&t, &a, compile, NULL))
		die("cannot start the compiler thread (256 MiB stack)");
	pthread_join(t, NULL);
	return 0;
}
static void *compile(void *unused) {
	(void)unused; int argc = g_argc; char **argv = g_argv;
	const char *out = "a.s", *in = NULL;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
		else if (!strcmp(argv[i], "-fPIC") || !strcmp(argv[i], "-fpic")) pic = 1;
		else if (!strcmp(argv[i], "-fno-PIC") || !strcmp(argv[i], "-fno-pic")   /* gcc: last one wins, so a later */
		      || !strcmp(argv[i], "-fno-PIE") || !strcmp(argv[i], "-fno-pie")) pic = 0;   /* negative CANCELS -fPIC */
		else if (!strcmp(argv[i], "-w")) diag_inhibit = 1;
		else if (!strcmp(argv[i], "-Werror")) diag_all(SEV_ERROR);
		else if (!strncmp(argv[i], "-Werror=", 8) && diag_set(argv[i] + 8, SEV_ERROR)) ;
		else if (!strncmp(argv[i], "-Wno-", 5) && diag_set(argv[i] + 5, SEV_OFF)) ;
		else if (argv[i][0] == '-' && argv[i][1] == 'W' && diag_set(argv[i] + 2, SEV_WARN)) ;
		else if (!strcmp(argv[i], "-mword-relocations")) word_relocs = 1;
		else if (!strncmp(argv[i], "-fvisibility=", 13)) {
			const char *v = argv[i] + 13;
			default_vis = !strcmp(v, "default") ? 0 : !strcmp(v, "hidden") ? VIS_HIDDEN : !strcmp(v, "internal") ? VIS_INTERNAL
			            : !strcmp(v, "protected") ? VIS_PROTECTED : (die("cc: unknown -fvisibility=%s", v), 0);
		}
		else if (!strcmp(argv[i], "-fgnu89-inline")) gnu89_inline = 1;
		else if (!strcmp(argv[i], "-fno-gnu89-inline")) gnu89_inline = 0;
		else if (!strcmp(argv[i], "-mfloat-abi=hard")) soft_float = softfp_abi = 0;
		else if (!strcmp(argv[i], "-mfloat-abi=soft") || !strcmp(argv[i], "-msoft-float")) soft_float = 1, softfp_abi = 0;   /* no floating
		                                                             * code allowed; no VFP ABI attributes (the kernel) */
		else if (!strcmp(argv[i], "-mfloat-abi=softfp")) die("cc: -mfloat-abi=softfp is not supported (VFP code with the base PCS)");
		else if (!strcmp(argv[i], "-mgeneral-regs-only")) general_regs_only = 1;
		else if (!strcmp(argv[i], "-funwind-tables") || !strcmp(argv[i], "-fasynchronous-unwind-tables")) unwind_tables = 1;
		else if (!strcmp(argv[i], "-fno-unwind-tables") || !strcmp(argv[i], "-fno-asynchronous-unwind-tables")) unwind_tables = 0;
		else if (!strcmp(argv[i], "-fshort-wchar")) short_wchar = 1; else if (!strcmp(argv[i], "-fno-short-wchar")) short_wchar = 0;
		else if (!strncmp(argv[i], "-ffixed-", 8)) {
			int r = fixed_reg_number(argv[i] + 8);
			if (r < 4 || r > 10) die("cc: %s: only r4-r10 can be left alone (the code generator works in r0-r3, ip, fp, lr)", argv[i]);
			fixed_regs |= 1u << r;
		}
		else if (!strcmp(argv[i], "-ffunction-sections")) function_sections = 1; else if (!strcmp(argv[i], "-fno-function-sections")) function_sections = 0;
		else if (!strcmp(argv[i], "-fdata-sections")) data_sections = 1; else if (!strcmp(argv[i], "-fno-data-sections")) data_sections = 0;
		else if (!strcmp(argv[i], "-march=armv7-a")) opt_march = "armv7-a";
		else if (!strcmp(argv[i], "-mcpu=cortex-a7")) opt_march = NULL;
		else if (!strncmp(argv[i], "-march=", 7) || !strncmp(argv[i], "-mcpu=", 6)) die("cc: unsupported %s (ARMv7-A: -march=armv7-a or -mcpu=cortex-a7)", argv[i]);
		else if (!strcmp(argv[i], "-mfpu=vfp") || !strcmp(argv[i], "-mfpu=neon-vfpv4") || !strcmp(argv[i], "-mfpu=vfpv4") || !strcmp(argv[i], "-mfpu=neon")) opt_mfpu = argv[i] + 6;
		else if (!strncmp(argv[i], "-mfpu=", 6)) die("cc: unsupported %s", argv[i]);
		else if (!strcmp(argv[i], "-marm")) ;   /* ARM state: all we generate */
		else if (!strcmp(argv[i], "-mno-unaligned-access")) no_unaligned = 1; else if (!strcmp(argv[i], "-munaligned-access")) no_unaligned = 0;
		else if (argv[i][0] == '-' && argv[i][1] == 'O') {   /* no optimizer here: the level only goes into the attributes */
			const char *l = argv[i] + 2;
			opt_olevel = !*l ? '1' : !strcmp(l, "fast") ? '3' : (l[0] == 's' || l[0] == 'z' || l[0] == 'g') && !l[1] ? l[0] : isdigit((unsigned char)l[0]) ? (atoi(l) > 3 ? '3' : l[0]) : (die("cc: unknown option '%s'", argv[i]), '0');
			if (!strcmp(l, "fast")) unsafe_math = finite_math = 1;
		}
		else if (!strcmp(argv[i], "-ffast-math") || !strcmp(argv[i], "-funsafe-math-optimizations")) unsafe_math = 1, finite_math |= argv[i][2] == 'f' && argv[i][3] == 'a';
		else if (!strcmp(argv[i], "-ffinite-math-only")) finite_math = 1;
		else if (!strcmp(argv[i], "-fno-fast-math")) unsafe_math = finite_math = 0;
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
	return NULL;
}
