/*
 * objdump.c — our from-scratch ELF structure dumper, the GNU `objdump` equivalent for the parts a build reads, byte
 * for byte: -t / -T (symbol tables), -h (section headers), -r / -R (relocations), on an ELF32 object, executable or
 * shared object, or on each member of an archive. There is NO disassembler: nothing in the kernel's or U-Boot's
 * normal build needs one (U-Boot wants `objdump -t` for u-boot.sym; the kernel's only -d use is its optional
 * module-disassembly target), so -d and the options that imply it are refused rather than faked.
 *
 * Everything printed follows BFD, because the output IS BFD's:
 *   - the banner "\nFILE:     file format elf32-littlearm\n" (an archive: "In archive AR:\n" first);
 *   - a symbol line (bfd_print_symbol_vandf + bfd_elf_print_symbol) is
 *         VALUE(8 hex) ' ' 7 flag chars ' ' SECTION '\t' SIZE(8 hex) [visibility] ' ' NAME
 *     where the flags are, in order: l/g/u/! (local/global/unique/both), w (weak), C (constructor), W (warning),
 *     I/i (indirect/ifunc), d/D (debugging/dynamic), F/f/O (function/file/object); SECTION is *UND* / *ABS* / *COM*
 *     or the section's name; SIZE is st_size — but a COMMON symbol's two columns swap, value showing st_size and
 *     size showing st_value (its alignment), as BFD stores them. ARM mapping symbols ($a, $d.foo, …) are hidden
 *     unless --special-syms, like GNU (bfd_is_target_special_symbol);
 *   - a section line is "%3d %-13s %08x  %08x  %08x  %08x  2**%u" (Idx Name Size VMA LMA "File off" Algn) then,
 *     indented 18, its BFD flags in BFD's order. The ELF->BFD flag mapping is _bfd_elf_make_section_from_shdr's:
 *     contents unless NOBITS, ALLOC/LOAD from SHF_ALLOC, READONLY unless SHF_WRITE, CODE from SHF_EXECINSTR else
 *     DATA when loaded, RELOC when some REL section targets it, and — only for a section with no SHF_ALLOC —
 *     DEBUGGING by NAME (.debug*, .zdebug*, .line*, .stab*, .gdb_index);
 *   - a relocation line is "%08x %-16s  %s" (offset, type, symbol), the type named from BFD's howto table (note
 *     those names are NOT readelf's for every type: BFD calls 25 and 26 R_ARM_GOTPC and R_ARM_GOT32, where the ELF
 *     ABI and readelf call them R_ARM_BASE_PREL and R_ARM_GOT_BREL); the symbol is its name — a section symbol's
 *     being its section's, and no symbol at all (index 0) reading "*ABS*" — followed by "@@version" where it has
 *     one, and "+0x%08x" when an addend is non-zero (never for ARM, whose relocations are REL).
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "elfread.h"
#include "arread.h"

void die(const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	fputs("objdump: ", stderr); vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap); exit(1);
}

#define SHT_NULL          0
#define SHT_NOTE          7
#define SHT_RELA          4
#define SHT_GROUP        17
#define SHF_GROUP      0x200u
#define SHF_EXCLUDE 0x80000000u
#define STB_GNU_UNIQUE   10
#define STT_GNU_IFUNC    10
#define STT_COMMON        5
#define SHN_LORESERVE 0xff00u
#define SHT_GNU_verdef  0x6ffffffdu
#define SHT_GNU_verneed 0x6ffffffeu
#define SHT_GNU_versym  0x6fffffffu
#define VER_FLG_BASE     0x1
#define VERSYM_HIDDEN 0x8000u

static int dump_syms, dump_dynsyms, dump_sections, dump_relocs, dump_dynrelocs, special_syms, wide_output;
static int exit_status;
static const char *only_sections[64]; static int nonly;

/* ---- BFD section flags (bfd/section.h), in dump_section_header's print order --------------------------- */
enum {
	SEC_HAS_CONTENTS = 1u << 0, SEC_ALLOC = 1u << 1, SEC_LOAD = 1u << 2, SEC_RELOC = 1u << 3,
	SEC_READONLY = 1u << 4, SEC_CODE = 1u << 5, SEC_DATA = 1u << 6, SEC_DEBUGGING = 1u << 7,
	SEC_EXCLUDE = 1u << 8, SEC_MERGE = 1u << 9, SEC_STRINGS = 1u << 10, SEC_THREAD_LOCAL = 1u << 11,
	SEC_GROUP = 1u << 12, SEC_ELF_OCTETS = 1u << 13, SEC_LINK_ONCE = 1u << 14, SEC_ELF_PURECODE = 1u << 15,
};
static int starts(const char *s, const char *p) { return !strncmp(s, p, strlen(p)); }
/* Does BFD make a section for this ELF one (bfd_section_from_shdr)? Relocation and symbol tables are consumed as
 * metadata, as are the section-name table and the symtab's string table — a dynsym's (.dynstr) does get a section.
 * -h lists exactly the ones it makes, numbered densely in that order. */
/* A REL/RELA section BFD cannot treat AS relocations — it presents it as an ordinary section instead, so it shows up
 * in -h and does not mark its target RELOC (bfd_section_from_shdr's SHT_REL case): one that the image loads (an
 * executable's or shared object's dynamic relocations), one against a symbol table other than the file's one
 * .symtab (so .dynsym's), or one with no usable target. */
static int rel_is_normal_section(const ElfFile *f, int i) {
	const Elf32_Shdr *h = &f->sh[i];
	int exec = f->eh->e_type == ET_EXEC || f->eh->e_type == ET_DYN;
	if (exec && (h->sh_flags & SHF_ALLOC)) return 1;
	if (h->sh_flags & 0x800u /* SHF_COMPRESSED */) return 1;
	if (!h->sh_link || (int)h->sh_link != f->symtab) return 1;
	if (!h->sh_info || (int)h->sh_info >= f->nsh) return 1;
	u32 t = f->sh[h->sh_info].sh_type;
	return t == SHT_REL || t == SHT_RELA;
}
static int bfd_has_section(const ElfFile *f, int i) {
	const Elf32_Shdr *h = &f->sh[i];
	if (h->sh_type == SHT_REL || h->sh_type == SHT_RELA) return rel_is_normal_section(f, i);
	if (h->sh_type == SHT_NULL || h->sh_type == SHT_SYMTAB || h->sh_type == 18 /* SHT_SYMTAB_SHNDX */) return 0;
	if (h->sh_type == SHT_STRTAB) {
		if (i == f->eh->e_shstrndx) return 0;
		for (int j = 1; j < f->nsh; j++) if (f->sh[j].sh_type == SHT_SYMTAB && (int)f->sh[j].sh_link == i) return 0;
	}
	return 1;
}
/* _bfd_elf_make_section_from_shdr, for what -h prints. has_rel: a REL/RELA section names this one. */
static unsigned sec_flags(const ElfFile *f, int i, int has_rel) {
	const Elf32_Shdr *h = &f->sh[i]; const char *name = f->shstr + h->sh_name;
	unsigned fl = 0;
	if (h->sh_type != SHT_NOBITS) fl |= SEC_HAS_CONTENTS;
	if (h->sh_type == SHT_GROUP) fl |= SEC_GROUP;
	if (h->sh_flags & SHF_ALLOC) { fl |= SEC_ALLOC; if (h->sh_type != SHT_NOBITS) fl |= SEC_LOAD; }
	if (!(h->sh_flags & SHF_WRITE)) fl |= SEC_READONLY;
	if (h->sh_flags & SHF_EXECINSTR) fl |= SEC_CODE; else if (fl & SEC_LOAD) fl |= SEC_DATA;
	if (h->sh_flags & SHF_MERGE) fl |= SEC_MERGE;
	if (h->sh_flags & SHF_STRINGS) fl |= SEC_STRINGS;
	if (h->sh_flags & SHF_TLS) fl |= SEC_THREAD_LOCAL;
	if (h->sh_flags & SHF_EXCLUDE) fl |= SEC_EXCLUDE;
	if (h->sh_flags & 0x20000000u /* SHF_ARM_PURECODE */) fl |= SEC_ELF_PURECODE;
	if (has_rel) fl |= SEC_RELOC;
	if (!(fl & SEC_ALLOC) && name[0] == '.') {   /* the debugging sections are recognised only by name */
		if (starts(name, ".debug") || starts(name, ".gnu.debuglto_.debug_") || starts(name, ".gnu.linkonce.wi.")
		    || starts(name, ".zdebug")) fl |= SEC_DEBUGGING | SEC_ELF_OCTETS;
		else if (starts(name, ".gnu.build.attributes") || starts(name, ".note.gnu")) fl |= SEC_ELF_OCTETS;
		else if (starts(name, ".line") || starts(name, ".stab") || !strcmp(name, ".gdb_index")) fl |= SEC_DEBUGGING;
	}
	if (starts(name, ".gnu.linkonce")) fl |= SEC_LINK_ONCE;
	return fl;
}

/* ---- the ARM relocation names BFD's howto table carries (bfd/elf32-arm.c) ------------------------------ */
static const struct { u32 type; const char *name; } arm_reloc_names[] = {
#include "arm-reloc-names.h"
};
static const char *reloc_name(u32 type) {
	for (size_t i = 0; i < sizeof arm_reloc_names / sizeof *arm_reloc_names; i++)
		if (arm_reloc_names[i].type == type) return arm_reloc_names[i].name;
	return NULL;
}

/* ---- symbol versions (.gnu.version + .gnu.version_d/_r) ----------------------------------------------- *
 * A file carrying a version symbol table AND some version definitions or needs gets a version column in every
 * symbol line — 13 characters wide, empty for an unversioned symbol (_bfd_elf_get_symbol_version_string, called by
 * bfd_elf_print_symbol with base_p). Index 0 has no version; index 1 is the base (the soname) and reads "Base";
 * anything up to the number of definitions names that definition; beyond them it is a NEEDED version, found by its
 * vna_other. VERSYM_HIDDEN marks a symbol bound to an old version, printed "(name)" instead. */
typedef struct { const u16 *versym; char **verdef; int nverdef; char **verneed; int nverneed; int active; } Versions;
static void versions_load(const ElfFile *f, Versions *v) {
	memset(v, 0, sizeof *v);
	int vs = 0, vd = 0, vr = 0;
	for (int i = 1; i < f->nsh; i++) {
		if (f->sh[i].sh_type == SHT_GNU_versym) vs = i;
		else if (f->sh[i].sh_type == SHT_GNU_verdef) vd = i;
		else if (f->sh[i].sh_type == SHT_GNU_verneed) vr = i;
	}
	if (!vs || (!vd && !vr)) return;
	v->active = 1;
	v->versym = (const u16 *)(f->data + f->sh[vs].sh_offset);
	if (vd) {                                        /* Elf32_Verdef { version, flags, ndx, cnt, hash, aux, next } */
		int strsec = (int)f->sh[vd].sh_link, n = 0;
		for (u32 off = 0; off < f->sh[vd].sh_size; ) {
			const u8 *d = f->data + f->sh[vd].sh_offset + off;
			u16 ndx = *(const u16 *)(d + 4); if (ndx > n) n = ndx;
			u32 next = *(const u32 *)(d + 16); if (!next) break;
			off += next;
		}
		v->nverdef = n; v->verdef = calloc((size_t)n + 1, sizeof *v->verdef);
		for (u32 off = 0; off < f->sh[vd].sh_size; ) {
			const u8 *d = f->data + f->sh[vd].sh_offset + off;
			u16 flags = *(const u16 *)(d + 2), ndx = *(const u16 *)(d + 4);
			u32 aux = *(const u32 *)(d + 12), next = *(const u32 *)(d + 16);
			if (ndx >= 1 && ndx <= n) {              /* its first Verdaux is the version's own name */
				const char *nm = elf_string(f, strsec, *(const u32 *)(d + aux));
				v->verdef[ndx] = strdup(ndx == 1 && (flags & VER_FLG_BASE) ? "Base" : nm);
			}
			if (!next) break;
			off += next;
		}
	}
	if (vr) {                                        /* Elf32_Verneed { version, cnt, file, aux, next } + Vernaux */
		int strsec = (int)f->sh[vr].sh_link, n = 0;
		for (int pass = 0; pass < 2; pass++) {
			for (u32 off = 0; off < f->sh[vr].sh_size; ) {
				const u8 *d = f->data + f->sh[vr].sh_offset + off;
				u16 cnt = *(const u16 *)(d + 2);
				u32 aux = *(const u32 *)(d + 8), next = *(const u32 *)(d + 12);
				for (u16 k = 0; k < cnt; k++) {
					const u8 *a = d + aux;
					u16 other = *(const u16 *)(a + 6) & (u16)~VERSYM_HIDDEN;
					if (!pass) { if (other > n) n = other; }
					else if (other >= 1 && other <= n) v->verneed[other] = strdup(elf_string(f, strsec, *(const u32 *)(a + 8)));
					u32 anext = *(const u32 *)(a + 12); if (!anext) break;
					aux += anext;
				}
				if (!next) break;
				off += next;
			}
			if (!pass) { v->nverneed = n; v->verneed = calloc((size_t)n + 1, sizeof *v->verneed); }
		}
	}
}
/* The version string for symbol index `i`, or NULL when the file carries no versions at all. */
static const char *version_of(const Versions *v, int i, int *hidden) {
	if (!v->active) return NULL;
	u16 raw = i ? v->versym[i] : 0; *hidden = (raw & VERSYM_HIDDEN) != 0;
	unsigned n = raw & ~VERSYM_HIDDEN;
	if (!n) return "";
	if ((int)n <= v->nverdef && v->verdef && v->verdef[n]) return v->verdef[n];
	if ((int)n <= v->nverneed && v->verneed && v->verneed[n]) return v->verneed[n];
	return "<corrupt>";
}

/* ---- symbols ------------------------------------------------------------------------------------------- */
static int arm_special(const char *n) {   /* bfd_is_arm_special_symbol_name(ANY): "$" + a lower-case letter, then end or "." */
	return n[0] == '$' && n[1] >= 'a' && n[1] <= 'z' && (n[2] == 0 || n[2] == '.');
}
/* bfd_print_symbol_vandf's 7 flag characters, from the ELF binding and type. */
static void sym_flags(char out[8], const Elf32_Sym *s, int dynamic) {
	int bind = ELF32_ST_BIND(s->st_info), type = ELF32_ST_TYPE(s->st_info);
	int local = bind == STB_LOCAL;
	/* STB_GLOBAL only becomes BSF_GLOBAL when the symbol is DEFINED and not common (elfcode.h) */
	int global = bind == STB_GLOBAL && s->st_shndx != SHN_UNDEF && s->st_shndx != SHN_COMMON;
	int debugging = type == STT_SECTION || type == STT_FILE;
	out[0] = local ? (global ? '!' : 'l') : global ? 'g' : bind == STB_GNU_UNIQUE ? 'u' : ' ';
	out[1] = bind == STB_WEAK ? 'w' : ' ';
	out[2] = ' ';                                    /* BSF_CONSTRUCTOR: never set for ELF */
	out[3] = ' ';                                    /* BSF_WARNING: never set for ELF     */
	out[4] = type == STT_GNU_IFUNC ? 'i' : ' ';      /* BSF_INDIRECT: never set for ELF    */
	out[5] = debugging ? 'd' : dynamic ? 'D' : ' ';
	out[6] = type == STT_FUNC ? 'F' : type == STT_FILE ? 'f'
	       : (type == STT_OBJECT || type == STT_COMMON) ? 'O' : ' ';
	out[7] = 0;
}
/* bfd_elf_sym_name: a nameless section symbol is called after its section. */
static const char *sym_name(const ElfFile *f, const Elf32_Sym *s, int strsec) {
	if (!s->st_name && ELF32_ST_TYPE(s->st_info) == STT_SECTION && s->st_shndx < f->nsh)
		return f->shstr + f->sh[s->st_shndx].sh_name;
	return elf_string(f, strsec, s->st_name);
}
static const char *sym_section(const ElfFile *f, const Elf32_Sym *s) {
	if (s->st_shndx == SHN_UNDEF) return "*UND*";
	if (s->st_shndx == SHN_ABS) return "*ABS*";
	if (s->st_shndx == SHN_COMMON) return "*COM*";
	if (s->st_shndx >= SHN_LORESERVE || s->st_shndx >= f->nsh) return "*ABS*";   /* BFD: "wrong, FIXME" */
	return f->shstr + f->sh[s->st_shndx].sh_name;
}
static void print_symbols(const ElfFile *f, int symsec, int dynamic) {
	Versions vers; versions_load(f, &vers);
	/* The column belongs to the FILE, not the table: .symtab's symbols carry no version index, so in a versioned
	 * file they print an empty one (13 spaces) — .gnu.version indexes the dynamic table only. */
	int versym_applies = dynamic;
	printf(dynamic ? "DYNAMIC SYMBOL TABLE:\n" : "SYMBOL TABLE:\n");
	if (!symsec) { printf("no symbols\n\n\n"); return; }
	const Elf32_Shdr *sh = &f->sh[symsec];
	const Elf32_Sym *sym = (const Elf32_Sym *)(f->data + sh->sh_offset);
	int n = (int)(sh->sh_size / sizeof *sym), strsec = (int)sh->sh_link;
	for (int i = 1; i < n; i++) {                    /* [0] is never printed */
		const Elf32_Sym *s = &sym[i];
		const char *name = sym_name(f, s, strsec);
		if (!special_syms && arm_special(name)) continue;
		int com = s->st_shndx == SHN_COMMON;
		char fl[8]; sym_flags(fl, s, dynamic);
		printf("%08x %s %s\t%08x", com ? s->st_size : s->st_value, fl, sym_section(f, s), com ? s->st_value : s->st_size);
		int vhidden = 0; const char *vs = version_of(&vers, versym_applies ? i : 0, &vhidden);
		if (vs && !vhidden) printf("  %-11s", vs);
		else if (vs) { printf(" (%s)", vs); for (int k = 10 - (int)strlen(vs); k > 0; k--) putchar(' '); }
		switch (ELF32_ST_VISIBILITY(s->st_other)) {
		case STV_DEFAULT: break;
		case STV_INTERNAL:  printf(" .internal");  break;
		case STV_HIDDEN:    printf(" .hidden");    break;
		case STV_PROTECTED: printf(" .protected"); break;
		default: printf(" 0x%02x", s->st_other); break;
		}
		printf(" %s\n", name);
	}
	printf("\n\n");
}

/* ---- sections ------------------------------------------------------------------------------------------ */
static int wanted(const char *name) {
	if (!nonly) return 1;
	for (int i = 0; i < nonly; i++) if (!strcmp(only_sections[i], name)) return 1;
	return 0;
}
/* ELF_SECTION_IN_SEGMENT (include/elf/internal.h), check_vma = 1, strict = 0. */
static int in_segment(const Elf32_Shdr *h, const Elf32_Phdr *p) {
	int tls = (h->sh_flags & SHF_TLS) != 0;
	if (!((tls && (p->p_type == PT_TLS || p->p_type == PT_GNU_RELRO || p->p_type == PT_LOAD))
	      || (!tls && p->p_type != PT_TLS && p->p_type != PT_PHDR))) return 0;
	if (!(h->sh_flags & SHF_ALLOC)
	    && (p->p_type == PT_LOAD || p->p_type == PT_DYNAMIC || p->p_type == 0x6474e550u /* EH_FRAME */
	        || p->p_type == 0x6474e551u /* STACK */ || p->p_type == 0x6474e552u /* RELRO */)) return 0;
	if (h->sh_type != SHT_NOBITS
	    && !(h->sh_offset >= p->p_offset && h->sh_offset - p->p_offset + h->sh_size <= p->p_filesz)) return 0;
	if ((h->sh_flags & SHF_ALLOC)
	    && !(h->sh_addr >= p->p_vaddr && h->sh_addr - p->p_vaddr + h->sh_size <= p->p_memsz)) return 0;
	return 1;
}
/* A section's load address, as _bfd_elf_make_section_from_shdr derives it: the segments are walked in order and
 * each containing one sets the LMA — from the FILE offset for a loaded section (so sections sharing a VMA, like a
 * kernel's copied .vectors.*, still get distinct LMAs), from the address otherwise; the walk stops at the segment
 * whose address range fully contains the section. All-zero p_paddr with more than one loaded PT_LOAD: LMA = VMA. */
static u32 sec_lma(const ElfFile *f, const Elf32_Shdr *h, unsigned flags) {
	if (!f->eh->e_phnum) return h->sh_addr;
	const Elf32_Phdr *ph = (const Elf32_Phdr *)(f->data + f->eh->e_phoff);
	int i, nload = 0;
	for (i = 0; i < f->eh->e_phnum; i++) {
		if (ph[i].p_paddr) break;
		if (ph[i].p_type == PT_LOAD && ph[i].p_memsz) nload++;
	}
	if (i >= f->eh->e_phnum && nload > 1) return h->sh_addr;
	u32 lma = h->sh_addr;
	for (i = 0; i < f->eh->e_phnum; i++) {
		if (!(((ph[i].p_type == PT_LOAD && !(h->sh_flags & SHF_TLS)) || ph[i].p_type == PT_TLS)
		      && in_segment(h, &ph[i]))) continue;
		lma = !(flags & SEC_LOAD) ? ph[i].p_paddr + h->sh_addr - ph[i].p_vaddr
		                          : ph[i].p_paddr + h->sh_offset - ph[i].p_offset;
		if (h->sh_addr >= ph[i].p_vaddr && h->sh_addr + h->sh_size <= ph[i].p_vaddr + ph[i].p_memsz) break;
	}
	return lma;
}
static int has_reloc_for(const ElfFile *f, int i) {
	for (int j = 1; j < f->nsh; j++)
		if ((f->sh[j].sh_type == SHT_REL || f->sh[j].sh_type == SHT_RELA) && (int)f->sh[j].sh_info == i
		    && !rel_is_normal_section(f, j)) return 1;
	return 0;
}
static void print_sections(const ElfFile *f) {
	int w = 13;
	if (wide_output) for (int i = 1; i < f->nsh; i++) {
		int l = (int)strlen(f->shstr + f->sh[i].sh_name);
		if (bfd_has_section(f, i) && wanted(f->shstr + f->sh[i].sh_name) && l > w) w = l;
	}
	printf("Sections:\n");
	printf("Idx %-*s Size      %-*s%-*sFile off  Algn", w, "Name", 10, "VMA", 10, "LMA");
	if (wide_output) printf("  Flags");
	printf("\n");
	static const struct { unsigned bit; const char *name; } order[] = {
		{ SEC_HAS_CONTENTS, "CONTENTS" }, { SEC_ALLOC, "ALLOC" }, { SEC_LOAD, "LOAD" }, { SEC_RELOC, "RELOC" },
		{ SEC_READONLY, "READONLY" }, { SEC_CODE, "CODE" }, { SEC_DATA, "DATA" }, { SEC_DEBUGGING, "DEBUGGING" },
		{ SEC_EXCLUDE, "EXCLUDE" }, { SEC_ELF_OCTETS, "OCTETS" }, { SEC_ELF_PURECODE, "PURECODE" },
		{ SEC_THREAD_LOCAL, "THREAD_LOCAL" }, { SEC_GROUP, "GROUP" },
	};
	int idx = 0;
	for (int i = 1; i < f->nsh; i++) {
		const Elf32_Shdr *h = &f->sh[i]; const char *name = f->shstr + h->sh_name;
		if (!bfd_has_section(f, i)) continue;
		if (!wanted(name)) { idx++; continue; }       /* (BFD numbers every section it made, filtered or not) */
		u32 align = h->sh_addralign; unsigned pow = 0;
		while (align > 1) { align >>= 1; pow++; }
		unsigned fl = sec_flags(f, i, has_reloc_for(f, i));
		printf("%3d %-*s %08x  %08x  %08x  %08x  2**%u", idx++, w, name, h->sh_size, h->sh_addr, sec_lma(f, h, fl), h->sh_offset, pow);
		if (!wide_output) printf("\n                ");
		printf("  ");
		const char *comma = "";
		for (size_t k = 0; k < sizeof order / sizeof *order; k++)
			if (fl & order[k].bit) { printf("%s%s", comma, order[k].name); comma = ", "; }
		if (fl & SEC_LINK_ONCE) printf("%sLINK_ONCE_DISCARD", comma);
		printf("\n");
	}
}

/* ---- relocations --------------------------------------------------------------------------------------- */
/* The relocations BFD could not attach to a target section are the DYNAMIC ones — the loader's, reported as one
 * merged table by -R; the rest patch the section sh_info names, reported per target section by -r. */
static int is_dynamic_rel(const ElfFile *f, int i) { return rel_is_normal_section(f, i); }
static void print_one_reloc_table(const ElfFile *f, const Elf32_Shdr *rs);
static void print_relocs(const ElfFile *f, int dynamic) {
	if (dynamic) {
		printf("DYNAMIC RELOCATION RECORDS");
		int dynsym = 0;
		for (int i = 1; i < f->nsh; i++) if (f->sh[i].sh_type == SHT_DYNSYM) dynsym = i;
		if (!dynsym) {   /* no dynamic symbol table: the relocations can't be read at all (GNU warns, see dump_elf) */
			printf("\n");
			fprintf(stderr, "objdump: failed to read relocs in: %s\n", f->path);
			fprintf(stderr, "objdump: error message was: invalid operation\n");
			return;
		}
		u32 total = 0;
		for (int i = 1; i < f->nsh; i++)
			if ((f->sh[i].sh_type == SHT_REL || f->sh[i].sh_type == SHT_RELA) && is_dynamic_rel(f, i))
				total += f->sh[i].sh_size;
		if (!total) { printf("\n"); return; }
		printf("\nOFFSET %*s TYPE %*s VALUE\n", 1, "", 12, "");
		for (int i = 1; i < f->nsh; i++)
			if ((f->sh[i].sh_type == SHT_REL || f->sh[i].sh_type == SHT_RELA) && is_dynamic_rel(f, i))
				print_one_reloc_table(f, &f->sh[i]);
		printf("\n\n");
		return;
	}
	for (int i = 1; i < f->nsh; i++) {
		const Elf32_Shdr *rs = &f->sh[i];
		if (rs->sh_type != SHT_REL && rs->sh_type != SHT_RELA) continue;
		if (is_dynamic_rel(f, i)) continue;
		int target = (int)rs->sh_info;
		if (target <= 0 || target >= f->nsh || !wanted(f->shstr + f->sh[target].sh_name)) continue;
		printf("RELOCATION RECORDS FOR [%s]:", f->shstr + f->sh[target].sh_name);
		if (!rs->sh_size) { printf(" (none)\n\n"); continue; }
		printf("\n");
		printf("OFFSET %*s TYPE %*s VALUE\n", 1, "", 12, "");
		print_one_reloc_table(f, rs);
		printf("\n\n");
	}
}
static void print_one_reloc_table(const ElfFile *f, const Elf32_Shdr *rs) {
	Versions vers; versions_load(f, &vers);
	int dynsym = 0;
	for (int i = 1; i < f->nsh; i++) if (f->sh[i].sh_type == SHT_DYNSYM) dynsym = i;
	if ((int)rs->sh_link != dynsym) vers.active = 0;   /* .gnu.version indexes .dynsym only */
	int entsz = rs->sh_type == SHT_RELA ? 12 : 8;
	u32 count = rs->sh_size / (u32)entsz;
	int strsec = 0; const Elf32_Sym *sym = NULL; int nsym = 0;
	if (rs->sh_link > 0 && (int)rs->sh_link < f->nsh) {
		const Elf32_Shdr *ss = &f->sh[rs->sh_link];
		sym = (const Elf32_Sym *)(f->data + ss->sh_offset); nsym = (int)(ss->sh_size / sizeof *sym);
		strsec = (int)ss->sh_link;
	}
	for (u32 k = 0; k < count; k++) {
		const Elf32_Rel *r = (const Elf32_Rel *)(f->data + rs->sh_offset + k * (u32)entsz);
		u32 off = r->r_offset, info = r->r_info;
		int si = (int)ELF32_R_SYM(info); u32 type = ELF32_R_TYPE(info);
		s32 addend = entsz == 12 ? *(const s32 *)((const u8 *)r + 8) : 0;   /* RELA's r_addend */
		printf("%08x", off);
		const char *rn = reloc_name(type);
		if (rn) printf(" %-16s  ", rn); else printf(" %-16u  ", type);
		const Elf32_Sym *s = sym && si > 0 && si < nsym ? &sym[si] : NULL;
		/* No symbol (index 0): BFD hands objdump its absolute-section symbol, named "*ABS*". A section
		 * symbol is named after its section (sym_name), and printed bare — the "[name]" form is for a
		 * relocation with no symbol at all, which an ELF file never has. */
		printf("%s", s ? sym_name(f, s, strsec) : "*ABS*");
		/* objdump_print_symname appends the version: "@@name", or "@name" when the symbol is bound to an
		 * old version or is undefined here. Section and synthetic symbols never carry one. */
		if (s && ELF32_ST_TYPE(s->st_info) != STT_SECTION) {
			int vhidden = 0; const char *vn = version_of(&vers, si, &vhidden);
			if (s->st_shndx == SHN_UNDEF) vhidden = 1;
			if (vn && *vn) printf(vhidden ? "@%s" : "@@%s", vn);
		}
		if (addend) printf(addend < 0 ? "-0x%08x" : "+0x%08x", addend < 0 ? (u32)-addend : (u32)addend);
		printf("\n");
	}
}

/* ---- one file ------------------------------------------------------------------------------------------ */
static u8 *slurp(const char *path, long *size) {
	FILE *fp = fopen(path, "rb"); if (!fp) die("'%s': No such file or directory", path);
	fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
	u8 *d = malloc((size_t)n + 4); if (!d) die("out of memory");
	if (n && fread(d, 1, (size_t)n, fp) != (size_t)n) die("%s: read failed", path);
	fclose(fp); *size = n; return d;
}
static int find_sym_section(const ElfFile *f, u32 type) {
	for (int i = 1; i < f->nsh; i++) if (f->sh[i].sh_type == type) return i;
	return 0;
}
static void dump_elf(const char *shown, u8 *data, long size) {
	ElfFile f; elf_open(&f, shown, data, size, (u16)(data[16] | data[17] << 8), 0);
	printf("\n%s:     file format elf32-littlearm\n", shown);
	printf("\n");                                      /* dump_bfd's one blank line; each dumper then spaces itself */
	/* slurp_dynamic_symtab: with no dynamic symbol table to read, a file that isn't a shared object/PIE at all is
	 * "not a dynamic object" and fails; one that is (U-Boot's image, whose script discards .dynsym) only warns. */
	int no_dynsym = !find_sym_section(&f, SHT_DYNSYM);
	if ((dump_dynsyms || dump_dynrelocs) && no_dynsym) {
		if (f.eh->e_type != ET_DYN) fprintf(stderr, "objdump: %s: not a dynamic object\n", shown);
		else fprintf(stderr, "objdump: %s: invalid operation\n", shown);
		exit_status = 1;                             /* (both paths fail the run: non_fatal + my_bfd_nonfatal) */
	}
	if (dump_sections)  print_sections(&f);
	if (dump_syms)      print_symbols(&f, find_sym_section(&f, SHT_SYMTAB), 0);
	if (dump_dynsyms)   print_symbols(&f, find_sym_section(&f, SHT_DYNSYM), 1);
	if (dump_relocs)    print_relocs(&f, 0);
	if (dump_dynrelocs) print_relocs(&f, 1);
}
static void dump_file(const char *path) {
	long size; u8 *data = slurp(path, &size);
	if (ar_is_archive(data, size)) {
		Archive ar; ar_open(&ar, path, data, size);
		printf("In archive %s:\n", path);
		long pos = 8; ArMember m;
		while (ar_next(&ar, &pos, &m)) {
			if (m.data) {
				u8 *copy = malloc((size_t)m.size + 4); if (!copy) die("out of memory");
				memcpy(copy, m.data, (size_t)m.size);
				dump_elf(m.name, copy, m.size); free(copy);
			} else {
				char *p = ar_member_path(&ar, &m); long n; u8 *d = slurp(p, &n);
				dump_elf(m.name, d, n); free(d); free(p);
			}
		}
		free(data);
		return;
	}
	dump_elf(path, data, size);
	free(data);
}

static const char *usage =
	"Usage: objdump <option(s)> <file(s)>\n"
	" Display information from object <file(s)>.\n"
	" At least one of the following switches must be given:\n"
	"  -h, --[section-]headers  Display the contents of the section headers\n"
	"  -t, --syms               Display the contents of the symbol table(s)\n"
	"  -T, --dynamic-syms       Display the contents of the dynamic symbol table\n"
	"  -r, --reloc              Display the relocation entries in the file\n"
	"  -R, --dynamic-reloc      Display the dynamic relocation entries in the file\n"
	"  -j, --section=NAME       Only display information for section NAME\n"
	"      --special-syms       Include special symbols in symbol dumps\n"
	"  -w, --wide               Format the output for more than 80 columns\n";

int main(int argc, char **argv) {
	const char *files[256]; int nfile = 0;
	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];
		if (!strcmp(a, "-t") || !strcmp(a, "--syms")) dump_syms = 1;
		else if (!strcmp(a, "-T") || !strcmp(a, "--dynamic-syms")) dump_dynsyms = 1;
		else if (!strcmp(a, "-h") || !strcmp(a, "--section-headers") || !strcmp(a, "--headers")) dump_sections = 1;
		else if (!strcmp(a, "-r") || !strcmp(a, "--reloc")) dump_relocs = 1;
		else if (!strcmp(a, "-R") || !strcmp(a, "--dynamic-reloc")) dump_dynrelocs = 1;
		else if (!strcmp(a, "--special-syms")) special_syms = 1;
		else if (!strcmp(a, "-w") || !strcmp(a, "--wide")) wide_output = 1;
		else if (!strcmp(a, "-j") || !strcmp(a, "--section")) {
			if (++i == argc) die("option '%s' requires an argument", a);
			if (nonly == 64) die("too many -j sections");
			only_sections[nonly++] = argv[i];
		}
		else if (!strncmp(a, "--section=", 10)) { if (nonly == 64) die("too many -j sections"); only_sections[nonly++] = a + 10; }
		else if (!strncmp(a, "-j", 2) && a[2]) { if (nonly == 64) die("too many -j sections"); only_sections[nonly++] = a + 2; }
		else if (!strcmp(a, "--help")) { fputs(usage, stdout); return 0; }
		else if (!strcmp(a, "--version")) { printf("objdump (os toolchain) 2.42\n"); return 0; }
		else if (!strcmp(a, "-d") || !strcmp(a, "--disassemble") || !strcmp(a, "-D") || !strcmp(a, "-S")
		         || !strcmp(a, "--disassemble-all") || !strcmp(a, "--source") || !strcmp(a, "-x") || !strcmp(a, "-f")
		         || !strcmp(a, "--all-headers") || !strcmp(a, "--file-headers"))
			die("option '%s' is not supported (no disassembler, and -f/-x would need the architecture name)", a);
		else if (a[0] == '-' && a[1]) die("unrecognized option '%s'", a);
		else { if (nfile == 256) die("too many files"); files[nfile++] = a; }
	}
	if (!nfile) { fputs(usage, stderr); die("no input file specified"); }
	if (!dump_syms && !dump_dynsyms && !dump_sections && !dump_relocs && !dump_dynrelocs)
		die("no information to dump: give -h, -t, -T, -r or -R");
	for (int i = 0; i < nfile; i++) dump_file(files[i]);
	return exit_status;
}
