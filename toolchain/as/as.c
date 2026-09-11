/*
 * as.c — the assembler FRONT-END (architecture-independent), the gas `read.c`/`as.c` equivalent.
 *
 * Reads a .s file, strips comments, and walks it line-by-line: peel labels, handle the GENERIC
 * directives every ELF target shares (.section/.global/.type/.size/.align/.word), and hand each
 * instruction to the machine-dependent backend via md_assemble(). It owns the symbol / section /
 * relocation / fixup tables + the byte buffers, exposing them to the backends as services (as.h);
 * it never encodes an instruction or writes an ELF byte itself. Arch-specific pseudo-ops fall through
 * to md_directive(); the ELF object is written by the obj backend (obj_write). One target is selected
 * at build time (we link exactly one md_* backend + one obj backend), exactly like GNU as.
 */
#define _POSIX_C_SOURCE 200809L   /* strdup */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include "as.h"

/* ------------------------------------------------------------------ tables (the shared model) ----- */
Section secs[MAXSEC]; int nsec; int cursec = -1;
Sym syms[MAXSYM]; int nsym;
Reloc rels[MAXREL]; int nrel;
Fixup fixes[MAXFIX]; int nfix;
static u32 local_last[10]; static int local_seen[10];   /* numeric local labels 0..9 */

void die(const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	fputs("as: ", stderr); vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap); exit(1);
}

int sec_find(const char *name) { for (int i = 0; i < nsec; i++) if (!strcmp(secs[i].name, name)) return i; return -1; }
int sec_get(const char *name, u32 type, u32 flags) {
	int i = sec_find(name); if (i >= 0) { cursec = i; return i; }
	if (nsec >= MAXSEC) die("too many sections");
	secs[nsec] = (Section){ strdup(name), type, flags, NULL, 0, 0, 0 };
	return cursec = nsec++;
}
void emit(const void *p, size_t n) {
	Section *s = &secs[cursec];
	if (s->len + n > s->cap) { s->cap = (s->len + n) * 2 + 64; s->data = realloc(s->data, s->cap); }
	memcpy(s->data + s->len, p, n); s->len += n;
}
void emit32(u32 w) { if (cursec < 0) die("instruction outside any section"); u8 b[4] = { w, w >> 8, w >> 16, w >> 24 }; emit(b, 4); }
void patch32(int sec, u32 off, u32 w) { u8 *d = secs[sec].data + off; d[0]=w; d[1]=w>>8; d[2]=w>>16; d[3]=w>>24; }
u32  read32(int sec, u32 off) { u8 *d = secs[sec].data + off; return d[0] | d[1]<<8 | d[2]<<16 | (u32)d[3]<<24; }
u32  here(void) { if (cursec < 0) die("instruction/label outside any section"); return secs[cursec].len; }

int sym_find(const char *name) { for (int i = 0; i < nsym; i++) if (syms[i].name && !strcmp(syms[i].name, name)) return i; return -1; }
int sym_intern(const char *name) {
	int i = sym_find(name); if (i >= 0) return i;
	if (nsym >= MAXSYM) die("too many symbols");
	syms[nsym] = (Sym){ strdup(name), 0, 0, 0, STB_GLOBAL, STT_NOTYPE, 0 };
	return nsym++;
}
void add_reloc(int sec, u32 off, int symidx, u32 type) { if (nrel >= MAXREL) die("too many relocations"); rels[nrel++] = (Reloc){ sec, off, symidx, type }; }
void add_fixup(int sec, u32 off, int local_num) { if (nfix >= MAXFIX) die("too many fixups"); fixes[nfix++] = (Fixup){ sec, off, local_num }; }
void local_define(int n, u32 value) { local_last[n] = value; local_seen[n] = 1; }
int  local_defined(int n) { return local_seen[n]; }
u32  local_value(int n) { return local_last[n]; }

/* ------------------------------------------------------------------ lexing ------------------------ */
/* Strip C-style block comments (which may span lines) + @ and // line comments, in place, replacing
   them with spaces while preserving newlines so line boundaries stay intact. */
static void strip_comments(char *s) {
	int in_block = 0;
	for (char *p = s; *p; p++) {
		if (in_block) { if (p[0] == '*' && p[1] == '/') { *p++ = ' '; *p = ' '; in_block = 0; } else if (*p != '\n') *p = ' '; continue; }
		if (p[0] == '/' && p[1] == '*') { *p++ = ' '; *p = ' '; in_block = 1; continue; }
		if (p[0] == '@' || (p[0] == '/' && p[1] == '/')) { while (*p && *p != '\n') *p++ = ' '; if (!*p) break; }
	}
}

/* Split a line into tokens on whitespace + commas ('#','{','}','[',']' stay attached to their operand). */
#define MAXTOK 32   /* mnemonic + operands; register lists (push/pop) can be long */
static char *toks[MAXTOK]; static int ntok; static char linebuf[512];
static void tokenize(const char *line) {
	ntok = 0; strncpy(linebuf, line, sizeof linebuf - 1); linebuf[sizeof linebuf - 1] = 0;
	char *p = linebuf;
	while (*p) {
		while (*p == ' ' || *p == '\t' || *p == ',') p++;
		if (!*p || *p == '\n') break;
		if (ntok >= MAXTOK) die("too many tokens on a line");
		toks[ntok++] = p;
		while (*p && *p != ' ' && *p != '\t' && *p != ',' && *p != '\n') p++;
		if (*p) *p++ = 0;
	}
}

/* ------------------------------------------------------------------ labels + directives ----------- */
static void def_label(const char *name) {
	if (cursec < 0) die("label '%s' outside any section", name);
	if (isdigit((unsigned char)name[0]) && name[1] == 0) { local_define(name[0] - '0', secs[cursec].len); return; }
	int i = sym_intern(name); syms[i].sec = cursec; syms[i].value = secs[cursec].len; syms[i].defined = 1;
}

/* GENERIC directives (shared by every ELF target). Arch pseudo-ops fall through to md_directive(). */
static void do_directive(void) {
	const char *d = toks[0];
	if (!strcmp(d, ".section")) {
		u32 flags = 0;
		if (ntok >= 3) { const char *f = toks[2];
			if (strchr(f, 'a')) flags |= SHF_ALLOC;
			if (strchr(f, 'x')) flags |= SHF_EXECINSTR;
			if (strchr(f, 'w')) flags |= SHF_WRITE; }
		sec_get(toks[1], SHT_PROGBITS, flags);
	} else if (!strcmp(d, ".text")) { sec_get(".text", SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR);
	} else if (!strcmp(d, ".data")) { sec_get(".data", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE);
	} else if (!strcmp(d, ".global") || !strcmp(d, ".globl")) { syms[sym_intern(toks[1])].bind = STB_GLOBAL;
	} else if (!strcmp(d, ".type")) { int i = sym_intern(toks[1]); if (toks[2] && strstr(toks[2], "function")) syms[i].type = STT_FUNC;
	} else if (!strcmp(d, ".size")) {
		/* .size <sym>, . - <label>  — the one expression form our startup asm needs. */
		int i = sym_intern(toks[1]);
		if (ntok >= 5 && !strcmp(toks[2], ".") && !strcmp(toks[3], "-")) {
			const char *l = toks[4]; u32 base;
			if (isdigit((unsigned char)l[0]) && l[1] == 'b' && l[2] == 0) { int n = l[0]-'0'; if (!local_defined(n)) die(".size: undefined %db", n); base = local_value(n); }
			else { int j = sym_find(l); if (j < 0 || !syms[j].defined) die(".size: undefined '%s'", l); base = syms[j].value; }
			syms[i].size = secs[cursec].len - base;
		}
	} else if (!strcmp(d, ".align") || !strcmp(d, ".p2align") || !strcmp(d, ".balign")) {
		if (cursec < 0) return;
		u32 a = ntok >= 2 ? (u32)strtol(toks[1], NULL, 0) : 2;
		u32 bytes = (!strcmp(d, ".balign")) ? a : (1u << a);
		while (bytes && (secs[cursec].len % bytes)) { u8 z = 0; emit(&z, 1); }
	} else if (!strcmp(d, ".word") || !strcmp(d, ".4byte")) {
		emit32((u32)strtol(toks[1], NULL, 0));
	} else if (!md_directive(toks, ntok)) {
		die("unknown directive '%s'", d);
	}
}

static void parse_line(char *line) {
	/* Peel leading labels ("_start:", "1:") — possibly several — then a directive or one instruction. */
	for (;;) {
		tokenize(line);
		if (ntok == 0) return;
		char *first = toks[0]; size_t n = strlen(first);
		if (n && first[n-1] == ':') {
			first[n-1] = 0; def_label(first);
			char *rest = strchr(line, ':'); line = rest ? rest + 1 : line + strlen(line);   /* re-tokenize remainder */
			continue;
		}
		break;
	}
	if (toks[0][0] == '.') do_directive();
	else md_assemble(toks, ntok);
}

static void resolve_fixups(void) {
	for (int i = 0; i < nfix; i++) {
		if (!local_defined(fixes[i].local_num)) die("unresolved forward local %df", fixes[i].local_num);
		md_apply_fix(&fixes[i]);
	}
}

/* ------------------------------------------------------------------ driver ------------------------ */
int main(int argc, char **argv) {
	const char *out = "a.out", *in = NULL;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
		else if (argv[i][0] != '-') in = argv[i];
		else die("unknown option '%s'", argv[i]);
	}
	if (!in) die("usage: as [-o out.o] in.s");

	FILE *f = fopen(in, "rb"); if (!f) die("cannot open %s", in);
	fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
	char *buf = malloc(sz + 1); if (fread(buf, 1, sz, f) != (size_t)sz) die("read failed"); buf[sz] = 0; fclose(f);

	strip_comments(buf);
	char *line = buf, *nl;
	do { nl = strchr(line, '\n'); if (nl) *nl = 0; parse_line(line); line = nl ? nl + 1 : NULL; } while (line);

	resolve_fixups();
	obj_write(out);
	return 0;
}
