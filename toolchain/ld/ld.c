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
/* Place allocatable sections at virtual addresses. One PT_LOAD from file offset 0 (so ehdr+phdr map
 * too); a section's vaddr == LOAD_BASE + its file offset (identity map). PROGBITS get file+memory;
 * NOBITS (.bss) get memory only, placed after all PROGBITS. Returns the file/memory end addresses. */
static void layout(u32 *filesz_end, u32 *memsz_end) {
	u32 hdrsz = sizeof(Elf32_Ehdr) + sizeof(Elf32_Phdr);   /* one program header */
	u32 cur = LOAD_BASE + hdrsz;
	for (int pass = 0; pass < 2; pass++) {                 /* pass 0 = PROGBITS, pass 1 = NOBITS */
		for (int i = 0; i < nobj; i++) for (int j = 0; j < objs[i].nsh; j++) {
			Elf32_Shdr *s = &objs[i].sh[j];
			int nobits = (s->sh_type == SHT_NOBITS);
			if (!(s->sh_flags & SHF_ALLOC) || !s->sh_size || nobits != (pass == 1)) continue;
			cur = alignup(cur, s->sh_addralign); objs[i].sec_vaddr[j] = cur; cur += s->sh_size;
		}
		if (pass == 0) *filesz_end = cur;
	}
	*memsz_end = cur;
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

	u32 filesz_end, memsz_end;
	layout(&filesz_end, &memsz_end);
	build_globals();
	GSym *start = gsym_find("_start");
	if (!start || !start->defined) die("no _start symbol (entry point)");
	relocate();
	elf_write_exec(out, start->vaddr, filesz_end, memsz_end);
	return 0;
}
