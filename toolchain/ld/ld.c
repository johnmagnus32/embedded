/*
 * ld.c — the linker FRONT-END (architecture- and format-generic linking algorithm) + driver.
 *
 * Produces a STATIC ELF executable from one or more relocatable objects, the inverse of the assembler:
 * `as` emitted section-relative bytes + relocations; `ld` assigns real virtual addresses and patches
 * them in. Four phases: load objects (elf.c) -> lay out allocatable sections at addresses -> resolve
 * the global symbol table -> apply relocations (via the md backend). The write-out is elf.c's job.
 *
 * The generic linking logic lives here; the ELF on-disk details are in elf.c and the per-relocation
 * encoding is in arm.c, so this file changes for neither a new object format nor a new architecture.
 * Milestone 1: one PT_LOAD, fixed base, PROGBITS/NOBITS only (no archives / dynamic linking yet).
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "ld.h"

Obj objs[MAXOBJ]; int nobj;

/* die() is tool-specific (its own "ld:" prefix); rd32/wr32/alignup/Strtab are shared (common/elfutil). */
void die(const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	fputs("ld: ", stderr); vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap); exit(1);
}

/* ---- global symbol table ------------------------------------------------------------------------- */
typedef struct { const char *name; u32 vaddr; int defined; } GSym;
#define MAXGSYM 4096
static GSym gsyms[MAXGSYM]; static int ngsym;
static GSym *gsym_find(const char *name) { for (int i = 0; i < ngsym; i++) if (!strcmp(gsyms[i].name, name)) return &gsyms[i]; return NULL; }
static void gsym_define(const char *name, u32 vaddr) {
	GSym *g = gsym_find(name);
	if (g) { if (g->defined) die("duplicate definition of '%s'", name); g->vaddr = vaddr; g->defined = 1; return; }
	if (ngsym >= MAXGSYM) die("too many global symbols");
	gsyms[ngsym++] = (GSym){ name, vaddr, 1 };
}

/* Resolve one object-local symbol index to a final virtual address. */
static u32 resolve(Obj *o, int symidx) {
	Elf32_Sym *s = &o->sym[symidx];
	if (s->st_shndx == SHN_UNDEF) {                     /* external — must be defined elsewhere */
		GSym *g = gsym_find(o->strtab + s->st_name);
		if (!g || !g->defined) die("undefined symbol '%s' (referenced in %s)", o->strtab + s->st_name, o->path);
		return g->vaddr;
	}
	if (s->st_shndx == SHN_ABS) return s->st_value;     /* absolute value, not relocated */
	return o->sec_vaddr[s->st_shndx] + s->st_value;     /* defined here (incl. STT_SECTION: value 0) */
}

/* ---- phases -------------------------------------------------------------------------------------- */
/* Assign every allocatable input section a virtual address, grouped into two page-aligned segments so
 * the kernel can map them with different permissions (W^X). Both segments keep vaddr == LOAD_BASE +
 * file-offset (the RW segment is bumped to a page boundary in file AND memory together, preserving that
 * identity), so the writer places each PROGBITS section at file offset vaddr - LOAD_BASE.
 *   seg 0 (R-X): headers + read-only sections   — SHF_ALLOC && !SHF_WRITE   (.text, .rodata)
 *   seg 1 (R-W): writable data then .bss         — SHF_ALLOC &&  SHF_WRITE   (.data [PROGBITS], .bss [NOBITS])
 * Three placement passes so sections of like kind are contiguous regardless of input order. */
static void place(int want_write, int nobits, u32 *cur) {
	for (int i = 0; i < nobj; i++) for (int j = 0; j < objs[i].nsh; j++) {
		Elf32_Shdr *s = &objs[i].sh[j];
		if (!(s->sh_flags & SHF_ALLOC) || !s->sh_size) continue;
		if (!!(s->sh_flags & SHF_WRITE) != want_write) continue;
		if ((s->sh_type == SHT_NOBITS) != nobits) continue;
		*cur = alignup(*cur, s->sh_addralign); objs[i].sec_vaddr[j] = *cur; *cur += s->sh_size;
	}
}
static void layout(Layout *L) {
	u32 hdrsz = sizeof(Elf32_Ehdr) + 2 * sizeof(Elf32_Phdr);   /* two program headers (R-X, R-W) */
	u32 cur = LOAD_BASE + hdrsz;
	place(0, 0, &cur);                       /* seg 0: read-only PROGBITS (.text, .rodata)          */
	L->rx_filesz = cur - LOAD_BASE;
	cur = LOAD_BASE + alignup(cur - LOAD_BASE, PAGE);   /* page-align the R-W segment (file + mem)   */
	L->rw_vaddr = cur; L->rw_off = cur - LOAD_BASE;
	place(1, 0, &cur);                       /* seg 1a: writable PROGBITS (.data) — on disk + memory */
	L->rw_filesz = cur - L->rw_vaddr;
	place(1, 1, &cur);                       /* seg 1b: .bss (NOBITS) — memory only, no file bytes    */
	L->rw_memsz = cur - L->rw_vaddr;
}

/* Record every defined global symbol's final address. */
static void build_globals(void) {
	for (int i = 0; i < nobj; i++) for (int k = 0; k < objs[i].nsym; k++) {
		Elf32_Sym *s = &objs[i].sym[k];
		if (ELF32_ST_BIND(s->st_info) == STB_GLOBAL && s->st_shndx != SHN_UNDEF && s->st_name)
			gsym_define(objs[i].strtab + s->st_name, objs[i].sec_vaddr[s->st_shndx] + s->st_value);
	}
}

/* For each REL section, patch its target section's bytes now that addresses are known. */
static void relocate(void) {
	for (int i = 0; i < nobj; i++) for (int j = 0; j < objs[i].nsh; j++) {
		Elf32_Shdr *rs = &objs[i].sh[j];
		if (rs->sh_type != SHT_REL) continue;
		Elf32_Shdr *ts = &objs[i].sh[rs->sh_info];              /* the section being patched */
		if (!(ts->sh_flags & SHF_ALLOC)) continue;
		Elf32_Rel *rel = (Elf32_Rel *)(objs[i].data + rs->sh_offset);
		int n = rs->sh_size / sizeof(Elf32_Rel);
		for (int r = 0; r < n; r++) {
			u32 S = resolve(&objs[i], ELF32_R_SYM(rel[r].r_info));       /* target symbol address   */
			u32 P = objs[i].sec_vaddr[rs->sh_info] + rel[r].r_offset;    /* address being patched   */
			u8 *loc = objs[i].data + ts->sh_offset + rel[r].r_offset;    /* bytes to patch          */
			md_apply_reloc(&objs[i], ELF32_R_TYPE(rel[r].r_info), loc, S, P);
		}
	}
}

int main(int argc, char **argv) {
	const char *out = "a.out";
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
		else if (argv[i][0] == '-') die("unknown option '%s'", argv[i]);
		else elf_load(argv[i]);
	}
	if (!nobj) die("usage: ld [-o out] obj.o ...");

	Layout L = {0};
	layout(&L);
	build_globals();
	GSym *start = gsym_find("_start");
	if (!start || !start->defined) die("no _start symbol (entry point)");
	relocate();
	elf_write_exec(out, start->vaddr, &L);
	return 0;
}
