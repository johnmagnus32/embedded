/*
 * readelf.c — our from-scratch ELF reporter, the GNU `readelf` equivalent for the parts a build reads, byte for
 * byte: -h (the file header), -S (section headers), -s (symbol tables), -r (relocations), -d (the dynamic table),
 * -n (notes), on an ELF32 object, executable or shared object, or on each member of an archive.
 *
 * Unlike objdump, readelf does NOT go through BFD: it reads the file and reports what is literally there. So there
 * are no synthetic symbols, no renumbered sections, no load addresses derived from segments — every number printed
 * is a field of the file. The one place that bites is relocation NAMES: readelf's come from the ELF ABI header
 * (include/elf/arm.h's RELOC_NUMBER list, arm-reloc-names.h here), and 7 of them differ from the BFD howto names
 * objdump prints — type 25 is R_ARM_BASE_PREL here and R_ARM_GOTPC there.
 *
 * Why these options: U-Boot's `checkarmreloc` runs `readelf -r -W` over its image and insists every relocation is
 * R_ARM_RELATIVE (its own startup code can only apply that one); the kernel's vdso install reads `readelf -n` for
 * a build ID. -h/-S/-s/-d come nearly free once the file is parsed, and are what one actually wants when checking
 * a layout by hand.
 *
 * Column widths are readelf's, and -W widens three of them (the relocation header, and the Name/Type fields of a
 * section line), so both forms are reproduced exactly.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "elfread.h"
#include "arread.h"
#include "elfver.h"

__attribute__((noreturn)) void die(const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	fputs("readelf: ", stderr); vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap); exit(1);
}

#define SHT_NULL           0
#define SHT_RELA           4
#define SHT_NOTE           7
#define SHT_INIT_ARRAY    14
#define SHT_FINI_ARRAY    15
#define SHT_PREINIT_ARRAY 16
#define SHT_GROUP         17
#define SHT_SYMTAB_SHNDX  18
#define SHT_GNU_ATTRIBUTES 0x6ffffff5u
#define SHT_GNU_HASH       0x6ffffff6u
#define SHT_GNU_LIBLIST    0x6ffffff7u
#define SHT_GNU_verdef     0x6ffffffdu
#define SHT_GNU_verneed    0x6ffffffeu
#define SHT_GNU_versym     0x6fffffffu
#define SHT_ARM_EXIDX      0x70000001u
#define SHT_ARM_PREEMPTMAP 0x70000002u
#define SHT_ARM_ATTRIBUTES 0x70000003u
#define SHF_MASKPROC    0xf0000000u
#define SHF_ARM_PURECODE 0x20000000u
#define SHF_GNU_RETAIN   0x200000u
#define SHF_GNU_MBIND    0x1000000u
#define SHF_COMPRESSED   0x800u
#define SHF_INFO_LINK    0x40u
#define SHF_OS_NONCONFORMING 0x100u
#define SHF_EXCLUDE   0x80000000u
#define STB_GNU_UNIQUE 10
#define STT_COMMON      5
#define STT_GNU_IFUNC  10
#define SHN_LORESERVE 0xff00u
#define SHN_XINDEX    0xffffu

static int do_header, do_sections, do_syms, do_relocs, do_dynamic, do_notes, wide;

/* ---- the ARM relocation names readelf prints --------------------------------------------------------- */
static const struct { u32 type; const char *name; } arm_relocs[] = {
#include "arm-reloc-names.h"
};
static const char *reloc_name(u32 type) {
	for (size_t i = 0; i < sizeof arm_relocs / sizeof *arm_relocs; i++)
		if (arm_relocs[i].type == type) return arm_relocs[i].name;
	return NULL;
}

/* ---- the name tables -------------------------------------------------------------------------------- */
static const char *section_type(u32 t) {
	switch (t) {
	case SHT_NULL: return "NULL";
	case SHT_PROGBITS: return "PROGBITS";
	case SHT_SYMTAB: return "SYMTAB";
	case SHT_STRTAB: return "STRTAB";
	case SHT_RELA: return "RELA";
	case SHT_HASH: return "HASH";
	case SHT_DYNAMIC: return "DYNAMIC";
	case SHT_NOTE: return "NOTE";
	case SHT_NOBITS: return "NOBITS";
	case SHT_REL: return "REL";
	case 11: return "DYNSYM";
	case SHT_INIT_ARRAY: return "INIT_ARRAY";
	case SHT_FINI_ARRAY: return "FINI_ARRAY";
	case SHT_PREINIT_ARRAY: return "PREINIT_ARRAY";
	case SHT_GROUP: return "GROUP";
	case SHT_SYMTAB_SHNDX: return "SYMTAB SECTION INDICES";
	case SHT_GNU_ATTRIBUTES: return "GNU_ATTRIBUTES";
	case SHT_GNU_HASH: return "GNU_HASH";
	case SHT_GNU_LIBLIST: return "GNU_LIBLIST";
	case SHT_GNU_verdef: return "VERDEF";
	case SHT_GNU_verneed: return "VERNEED";
	case SHT_GNU_versym: return "VERSYM";
	case SHT_ARM_EXIDX: return "ARM_EXIDX";
	case SHT_ARM_PREEMPTMAP: return "ARM_PREEMPTMAP";
	case SHT_ARM_ATTRIBUTES: return "ARM_ATTRIBUTES";
	default: return NULL;
	}
}
static const char *sym_type_name(u32 t) {
	switch (t) {
	case STT_NOTYPE: return "NOTYPE";
	case STT_OBJECT: return "OBJECT";
	case STT_FUNC: return "FUNC";
	case STT_SECTION: return "SECTION";
	case STT_FILE: return "FILE";
	case STT_COMMON: return "COMMON";
	case STT_TLS: return "TLS";
	case STT_GNU_IFUNC: return "GNU_IFUNC";
	default: return NULL;
	}
}
static const char *sym_bind_name(u32 b) {
	switch (b) {
	case STB_LOCAL: return "LOCAL";
	case STB_GLOBAL: return "GLOBAL";
	case STB_WEAK: return "WEAK";
	case STB_GNU_UNIQUE: return "UNIQUE";
	default: return NULL;
	}
}
static const char *sym_vis_name(u32 v) {
	switch (v) {
	case STV_DEFAULT: return "DEFAULT";
	case STV_INTERNAL: return "INTERNAL";
	case STV_HIDDEN: return "HIDDEN";
	case STV_PROTECTED: return "PROTECTED";
	default: return "DEFAULT";
	}
}
/* The dynamic tags, with how readelf renders each value: 'p' a pointer (0xHEX), 'b' a byte count "N (bytes)",
 * 's' a .dynstr offset, 'v' a plain number, 'r' DT_PLTREL's REL/RELA, 'i' DT_FLAGS-style, 'n' a name in brackets. */
static const struct { u32 tag; const char *name; char kind; } dyn_tags[] = {
	{ 0, "NULL", 'p' }, { 1, "NEEDED", 'n' }, { 2, "PLTRELSZ", 'b' }, { 3, "PLTGOT", 'p' },
	{ 4, "HASH", 'p' }, { 5, "STRTAB", 'p' }, { 6, "SYMTAB", 'p' }, { 7, "RELA", 'p' },
	{ 8, "RELASZ", 'b' }, { 9, "RELAENT", 'b' }, { 10, "STRSZ", 'b' }, { 11, "SYMENT", 'b' },
	{ 12, "INIT", 'p' }, { 13, "FINI", 'p' }, { 14, "SONAME", 's' }, { 15, "RPATH", 's' },
	{ 16, "SYMBOLIC", 'p' }, { 17, "REL", 'p' }, { 18, "RELSZ", 'b' }, { 19, "RELENT", 'b' },
	{ 20, "PLTREL", 'r' }, { 21, "DEBUG", 'p' }, { 22, "TEXTREL", 'p' }, { 23, "JMPREL", 'p' },
	{ 24, "BIND_NOW", 'p' }, { 25, "INIT_ARRAY", 'p' }, { 26, "FINI_ARRAY", 'p' },
	{ 27, "INIT_ARRAYSZ", 'b' }, { 28, "FINI_ARRAYSZ", 'b' }, { 29, "RUNPATH", 's' },
	{ 30, "FLAGS", 'i' }, { 32, "PREINIT_ARRAY", 'p' }, { 33, "PREINIT_ARRAYSZ", 'v' },
	{ 0x6ffffef5u, "GNU_HASH", 'p' }, { 0x6ffffff0u, "VERSYM", 'p' },
	{ 0x6ffffff9u, "RELACOUNT", 'v' }, { 0x6ffffffau, "RELCOUNT", 'v' },
	{ 0x6ffffffbu, "FLAGS_1", 'i' }, { 0x6ffffffcu, "VERDEF", 'p' }, { 0x6ffffffdu, "VERDEFNUM", 'v' },
	{ 0x6ffffffeu, "VERNEED", 'p' }, { 0x6fffffffu, "VERNEEDNUM", 'v' },
};

/* ---- -h: the file header ----------------------------------------------------------------------------- */
static void print_header(const ElfFile *f) {
	const Elf32_Ehdr *e = f->eh;
	printf("ELF Header:\n");
	printf("  Magic:   ");
	for (int i = 0; i < 16; i++) printf("%2.2x ", f->data[i]);
	printf("\n");
	printf("  %-34s %s\n", "Class:", "ELF32");
	printf("  %-34s %s\n", "Data:", "2's complement, little endian");
	printf("  %-34s %u%s\n", "Version:", f->data[6], f->data[6] == 1 ? " (current)" : "");
	printf("  %-34s %s\n", "OS/ABI:", f->data[7] == 0 ? "UNIX - System V" : "<unknown>");
	printf("  %-34s %u\n", "ABI Version:", f->data[8]);
	const char *ty = e->e_type == 0 ? "NONE (None)" : e->e_type == ET_REL ? "REL (Relocatable file)"
	               : e->e_type == ET_EXEC ? "EXEC (Executable file)" : e->e_type == ET_DYN ? "DYN (Shared object file)"
	               : e->e_type == 4 ? "CORE (Core file)" : "<unknown>";
	printf("  %-34s %s\n", "Type:", ty);
	printf("  %-34s %s\n", "Machine:", e->e_machine == EM_ARM ? "ARM" : "<unknown>");
	printf("  %-34s 0x%x\n", "Version:", e->e_version);
	printf("  %-34s 0x%x\n", "Entry point address:", e->e_entry);
	printf("  %-34s %u (bytes into file)\n", "Start of program headers:", e->e_phoff);
	printf("  %-34s %u (bytes into file)\n", "Start of section headers:", e->e_shoff);
	/* decode_ARM_machine_flags: the EABI version, then the remaining bits lowest-first. */
	printf("  %-34s 0x%x", "Flags:", e->e_flags);
	u32 fl = e->e_flags, ver = fl & 0xff000000u;
	fl &= ~0xff000000u;
	if (fl & 0x1u) { printf(", relocatable executable"); fl &= ~0x1u; }
	if (ver == 0x05000000u) {
		printf(", Version5 EABI");
		while (fl) {
			u32 bit = fl & (u32)-(s32)fl; fl &= ~bit;
			if (bit == 0x00800000u) printf(", BE8");
			else if (bit == 0x00400000u) printf(", LE8");
			else if (bit == 0x00000200u) printf(", soft-float ABI");
			else if (bit == 0x00000400u) printf(", hard-float ABI");
			else { printf(" <unknown>"); break; }
		}
	} else if (ver == 0x04000000u) printf(", Version4 EABI");
	else if (ver == 0x03000000u) printf(", Version3 EABI");
	else if (ver) printf(", <unrecognized EABI>");
	printf("\n");
	printf("  %-34s %u (bytes)\n", "Size of this header:", e->e_ehsize);
	printf("  %-34s %u (bytes)\n", "Size of program headers:", e->e_phentsize);
	printf("  %-34s %u\n", "Number of program headers:", e->e_phnum);
	printf("  %-34s %u (bytes)\n", "Size of section headers:", e->e_shentsize);
	printf("  %-34s %u\n", "Number of section headers:", e->e_shnum);
	printf("  %-34s %u\n", "Section header string table index:", e->e_shstrndx);
}

/* print_symbol_name — a name into a COLUMN budget, as readelf prints every one. Unprintable bytes are escaped and
 * cost more than one column: a control character is "^X" (2), anything else non-printable "<XX>" (4). If the name
 * is longer than the budget it is cut and "[...]" appended (the budget loses those 5 columns first). -W removes
 * the budget entirely. A NEGATIVE width also pads the field out to its full size. Budgets: 21 for a symbol (-s,
 * less the version suffix's columns), 17 for a section name (-S, padded), 22 for a relocation's symbol.
 * Returns the columns emitted. */
static int print_symbol_name(int width, const char *name) {
	int pad = 0, printed = 0, budget;
	if (width < 0) { width = -width; pad = 1; }
	else if (!width) return 0;
	if (wide) budget = 1 << 30;
	else {
		budget = width;
		if ((int)strlen(name) > width) { budget -= 5; if (budget < 0) budget = 0; }
	}
	for (const unsigned char *c = (const unsigned char *)name; *c && budget > 0; c++) {
		if (*c >= 0x20 && *c < 0x7f) { putchar(*c); budget--; printed++; }
		else if (*c < 0x20) { if (budget < 2) break; printf("^%c", *c + 0x40); budget -= 2; printed += 2; }
		else { if (budget < 4) break; printf("<%02X>", *c); budget -= 4; printed += 4; }
	}
	if (!wide && (int)strlen(name) > width) { printed += printf("[...]"); }
	if (pad) for (; printed < width; printed++) putchar(' ');
	return printed;
}

/* ---- -S: the section headers -------------------------------------------------------------------------- */
/* readelf's get_elf_section_flags: one letter per flag, in this order, then the unknown/OS/processor marks. */
static void section_flag_letters(u32 fl, char *out) {
	char *p = out;
	if (fl & SHF_WRITE) *p++ = 'W';
	if (fl & SHF_ALLOC) *p++ = 'A';
	if (fl & SHF_EXECINSTR) *p++ = 'X';
	if (fl & SHF_MERGE) *p++ = 'M';
	if (fl & SHF_STRINGS) *p++ = 'S';
	if (fl & SHF_INFO_LINK) *p++ = 'I';
	if (fl & SHF_LINK_ORDER) *p++ = 'L';
	if (fl & SHF_OS_NONCONFORMING) *p++ = 'O';
	if (fl & 0x200u /* SHF_GROUP */) *p++ = 'G';
	if (fl & SHF_TLS) *p++ = 'T';
	if (fl & SHF_COMPRESSED) *p++ = 'C';
	if (fl & SHF_EXCLUDE) *p++ = 'E';
	if (fl & SHF_GNU_MBIND) *p++ = 'D';
	if (fl & SHF_ARM_PURECODE) *p++ = 'y';
	if (fl & SHF_GNU_RETAIN) *p++ = 'R';
	*p = 0;
}
static void print_sections(const ElfFile *f) {
	const Elf32_Ehdr *e = f->eh;
	if (!e->e_shnum) { printf("\nThere are no sections in this file.\n"); return; }
	/* -h has already said how many and where, so -S repeats it only on its own */
	if (!do_header) printf("There are %u section headers, starting at offset 0x%x:\n", e->e_shnum, e->e_shoff);
	printf("\nSection Headers:\n");
	if (wide) printf("  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al\n");
	else      printf("  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al\n");
	for (int i = 0; i < f->nsh; i++) {
		const Elf32_Shdr *h = &f->sh[i];
		const char *name = f->shstr + h->sh_name, *tn = section_type(h->sh_type);
		char tbuf[32], flags[20];
		if (!tn) { snprintf(tbuf, sizeof tbuf, "LOPROC+%x", h->sh_type - 0x70000000u); tn = tbuf; }
		section_flag_letters(h->sh_flags, flags);
		printf("  [%2d] ", i);
		print_symbol_name(-17, name);
		printf(" %-15s %08x %06x %06x %02x %3s %2u %3u %2u\n", tn,
		       h->sh_addr, h->sh_offset, h->sh_size, h->sh_entsize, flags, h->sh_link, h->sh_info, h->sh_addralign);
	}
	printf("Key to Flags:\n"
	       "  W (write), A (alloc), X (execute), M (merge), S (strings), I (info),\n"
	       "  L (link order), O (extra OS processing required), G (group), T (TLS),\n"
	       "  C (compressed), x (unknown), o (OS specific), E (exclude),\n"
	       "  D (mbind), y (purecode), p (processor specific)\n");
}



/* ---- -s: the symbol tables ---------------------------------------------------------------------------- */
static void print_symtab(const ElfFile *f, int i) {
	const Elf32_Shdr *sh = &f->sh[i];
	const Elf32_Sym *sym = (const Elf32_Sym *)(f->data + sh->sh_offset);
	u32 n = sh->sh_size / (u32)sizeof *sym;
	int strsec = (int)sh->sh_link, dynamic = sh->sh_type == SHT_DYNSYM;
	Versions vers; versions_load(f, &vers);
	printf("\nSymbol table '%s' contains %u %s:\n", f->shstr + sh->sh_name, n, n == 1 ? "entry" : "entries");
	printf("   Num:    Value  Size Type    Bind   Vis      Ndx Name\n");
	for (u32 k = 0; k < n; k++) {
		const Elf32_Sym *s = &sym[k];
		u32 t = ELF32_ST_TYPE(s->st_info), b = ELF32_ST_BIND(s->st_info);
		const char *tn = sym_type_name(t), *bn = sym_bind_name(b);
		char tbuf[20], bbuf[20], ndx[12];
		if (!tn) { snprintf(tbuf, sizeof tbuf, "<unknown>: %u", t); tn = tbuf; }
		if (!bn) { snprintf(bbuf, sizeof bbuf, "<unknown>: %u", b); bn = bbuf; }
		if (s->st_shndx == SHN_UNDEF) snprintf(ndx, sizeof ndx, "UND");
		else if (s->st_shndx == SHN_ABS) snprintf(ndx, sizeof ndx, "ABS");
		else if (s->st_shndx == SHN_COMMON) snprintf(ndx, sizeof ndx, "COM");
		else snprintf(ndx, sizeof ndx, "%u", s->st_shndx);
		/* a nameless section symbol reads as its section (readelf's printable_section_name path) */
		const char *nm = elf_string(f, strsec, s->st_name);
		if (!s->st_name && t == STT_SECTION && s->st_shndx < f->nsh) nm = f->shstr + f->sh[s->st_shndx].sh_name;

		/* print_dynamic_symbol's widths: type %-7s, bind %-6s, visibility %-7s, then the index " %4s " —
		 * so an over-long visibility (PROTECTED) pushes the row right rather than eating the gap. The SIZE is
		 * decimal while it fits 5 columns, hex beyond (print_vma's DEC_5). */
		int vhidden = 0; const char *vn = dynamic ? version_of(&vers, (int)k, &vhidden) : NULL;
		if (vn && (!*vn || !strcmp(vn, nm))) vn = NULL;   /* a version-definition symbol names itself */
		printf("%6u: %08x ", k, s->st_value);
		if (s->st_size <= 99999) printf("%5u", s->st_size); else printf("0x%x", s->st_size);
		printf(" %-7s %-6s %-7s %4s ", tn, bn, sym_vis_name(ELF32_ST_VISIBILITY(s->st_other)), ndx);
		int budget = 21;
		if (!wide && vn) { budget -= 1 + (int)strlen(vn); if (!vhidden) budget -= 1; }
		print_symbol_name(budget, nm);
		if (vn) printf(vhidden ? "@%s" : "@@%s", vn);
		printf("\n");
	}
}
static void print_syms(const ElfFile *f) {
	int any = 0;
	for (int i = 1; i < f->nsh; i++)
		if (f->sh[i].sh_type == SHT_SYMTAB || f->sh[i].sh_type == SHT_DYNSYM) { print_symtab(f, i); any = 1; }
	if (!any) printf("\nThere are no symbols in this file.\n");
}

/* ---- -r: the relocations ------------------------------------------------------------------------------ */
static void print_relocs(const ElfFile *f) {
	int any = 0;
	for (int i = 1; i < f->nsh; i++) {
		const Elf32_Shdr *rs = &f->sh[i];
		if (rs->sh_type != SHT_REL && rs->sh_type != SHT_RELA) continue;
		any = 1;
		int entsz = rs->sh_type == SHT_RELA ? 12 : 8;
		u32 n = rs->sh_size / (u32)entsz;
		printf("\nRelocation section '%s' at offset 0x%x contains %u %s:\n",
		       f->shstr + rs->sh_name, rs->sh_offset, n, n == 1 ? "entry" : "entries");
		if (wide) printf(" Offset     Info    Type                Sym. Value  Symbol's Name%s\n", entsz == 12 ? " + Addend" : "");
		else      printf(" Offset     Info    Type            Sym.Value  Sym. Name%s\n", entsz == 12 ? " + Addend" : "");
		const Elf32_Sym *sym = NULL; int nsym = 0, strsec = 0, dynamic = 0;
		Versions vers; versions_load(f, &vers);
		if (rs->sh_link > 0 && (int)rs->sh_link < f->nsh) {
			const Elf32_Shdr *ss = &f->sh[rs->sh_link];
			sym = (const Elf32_Sym *)(f->data + ss->sh_offset); nsym = (int)(ss->sh_size / sizeof *sym);
			strsec = (int)ss->sh_link; dynamic = ss->sh_type == SHT_DYNSYM;
		}
		for (u32 k = 0; k < n; k++) {
			const Elf32_Rel *r = (const Elf32_Rel *)(f->data + rs->sh_offset + k * (u32)entsz);
			int si = (int)ELF32_R_SYM(r->r_info); u32 type = ELF32_R_TYPE(r->r_info);
			const char *rn = reloc_name(type); char rbuf[24];
			if (!rn) { snprintf(rbuf, sizeof rbuf, "unrecognized: %-7x", type); rn = rbuf; }
			printf("%08x  %08x %-*s", r->r_offset, r->r_info, wide ? 22 : 17, rn);
			if (si && si < nsym) {
				const Elf32_Sym *s = &sym[si];
				const char *nm = elf_string(f, strsec, s->st_name);
				if (!s->st_name && ELF32_ST_TYPE(s->st_info) == STT_SECTION && s->st_shndx < f->nsh)
					nm = f->shstr + f->sh[s->st_shndx].sh_name;
				int vhidden = 0; const char *vn = dynamic ? version_of(&vers, si, &vhidden) : NULL;
				if (vn && (!*vn || !strcmp(vn, nm))) vn = NULL;
				printf(" %08x   ", s->st_value);
				print_symbol_name(22, nm);
				if (vn) printf(vhidden ? "@%s" : "@@%s", vn);
			}
			if (entsz == 12) printf(" + %x", *(const u32 *)((const u8 *)r + 8));
			printf("\n");
		}
	}
	if (!any) printf("\nThere are no relocations in this file.\n");
}

/* ---- -d: the dynamic table ---------------------------------------------------------------------------- */
static void print_dynamic(const ElfFile *f) {
	int dyn = 0;
	for (int i = 1; i < f->nsh; i++) if (f->sh[i].sh_type == SHT_DYNAMIC) dyn = i;
	if (!dyn) { printf("\nThere is no dynamic section in this file.\n"); return; }
	const Elf32_Shdr *sh = &f->sh[dyn];
	const Elf32_Dyn *d = (const Elf32_Dyn *)(f->data + sh->sh_offset);
	u32 n = sh->sh_size / (u32)sizeof *d, count = 0;
	while (count < n && d[count].d_tag != 0) count++;
	if (count < n) count++;                              /* the terminating DT_NULL is counted too */
	int strsec = (int)sh->sh_link;
	printf("\nDynamic section at offset 0x%x contains %u %s:\n", sh->sh_offset, count, count == 1 ? "entry" : "entries");
	printf("  Tag        Type                         Name/Value\n");
	for (u32 k = 0; k < count; k++) {
		const char *name = NULL; char kind = 'p';
		for (size_t j = 0; j < sizeof dyn_tags / sizeof *dyn_tags; j++)
			if (dyn_tags[j].tag == d[k].d_tag) { name = dyn_tags[j].name; kind = dyn_tags[j].kind; }
		char tbuf[32];
		if (!name) { snprintf(tbuf, sizeof tbuf, "0x%x", d[k].d_tag); name = tbuf; }
		/* readelf: printf(" (%s)%*s", type, 27 - strlen(type), " ") for 32-bit — a right-aligned single space */
		printf(" 0x%8.8x (%s)%*s", d[k].d_tag, name, 27 - (int)strlen(name), " ");
		switch (kind) {
		case 'b': printf("%u (bytes)\n", d[k].d_val); break;
		case 'v': printf("%u\n", d[k].d_val); break;
		case 'r': printf("%s\n", d[k].d_val == 17 ? "REL" : d[k].d_val == 7 ? "RELA" : "<unknown>"); break;
		case 'n': printf("Shared library: [%s]\n", elf_string(f, strsec, d[k].d_val)); break;
		case 's': printf("Library soname: [%s]\n", elf_string(f, strsec, d[k].d_val)); break;
		default:  printf("0x%x\n", d[k].d_val); break;
		}
	}
}

/* ---- -n: the notes ------------------------------------------------------------------------------------ */
static void print_notes(const ElfFile *f) {
	for (int i = 1; i < f->nsh; i++) {
		const Elf32_Shdr *sh = &f->sh[i];
		if (sh->sh_type != SHT_NOTE) continue;
		printf("\nDisplaying notes found in: %s\n", f->shstr + sh->sh_name);
		printf("  Owner                Data size \tDescription\n");
		for (u32 off = 0; off + 12 <= sh->sh_size; ) {
			const u8 *p = f->data + sh->sh_offset + off;
			u32 namesz = *(const u32 *)p, descsz = *(const u32 *)(p + 4), type = *(const u32 *)(p + 8);
			const char *owner = (const char *)(p + 12);
			u32 nextoff = off + 12 + ((namesz + 3) & ~3u) + ((descsz + 3) & ~3u);
			if (nextoff > sh->sh_size) break;
			/* get_note_type: the names depend on the OWNER — "GNU" has its own set, every other owner
			 * (a kernel's "Linux" notes, say) reads from the generic one. */
			int gnu = !strcmp(owner, "GNU");
			const char *desc = gnu ? (type == 1 ? "NT_GNU_ABI_TAG (ABI version tag)"
			                        : type == 2 ? "NT_GNU_HWCAP (DSO-supplied software HWCAP info)"
			                        : type == 3 ? "NT_GNU_BUILD_ID (unique build ID bitstring)"
			                        : type == 4 ? "NT_GNU_GOLD_VERSION (gold version)"
			                        : type == 5 ? "NT_GNU_PROPERTY_TYPE_0" : NULL)
			                       : (type == 1 ? "NT_VERSION (version)"
			                        : type == 2 ? "NT_ARCH (architecture)"
			                        : type == 0x100 ? "OPEN" : type == 0x101 ? "func" : NULL);
			char dbuf[64];
			if (!desc) { snprintf(dbuf, sizeof dbuf, "Unknown note type: (0x%08x)", type); desc = dbuf; }
			printf("  %-20s 0x%08x\t%s\n", owner, descsz, desc);
			const u8 *data = p + 12 + ((namesz + 3) & ~3u);
			if (gnu && type == 3) {
				printf("    Build ID: ");
				for (u32 b = 0; b < descsz; b++) printf("%02x", data[b]);
				printf("\n");
			} else if (descsz) {                     /* print_note_contents_hex: anything not decoded above */
				printf("   description data: ");
				for (u32 b = 0; b < descsz; b++) printf("%02x ", data[b]);
				if (!wide) printf("\n");
			}
			if (wide && !(gnu && type == 3)) printf("\n");
			off = nextoff;
		}
	}
}

/* ---- one file ----------------------------------------------------------------------------------------- */
static u8 *slurp(const char *path, long *size) {
	FILE *fp = fopen(path, "rb"); if (!fp) die("Error: '%s': No such file", path);
	fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
	u8 *d = malloc((size_t)n + 4); if (!d) die("out of memory");
	if (n && fread(d, 1, (size_t)n, fp) != (size_t)n) die("%s: read failed", path);
	fclose(fp); *size = n; return d;
}
static void dump_elf(u8 *data, long size, const char *path) {
	ElfFile f; elf_open(&f, path, data, size, (u16)(data[16] | data[17] << 8), 0);
	if (do_header)   print_header(&f);
	if (do_sections) print_sections(&f);
	if (do_dynamic)  print_dynamic(&f);                /* readelf's order: ... dynamic, relocs, symbols, notes */
	if (do_relocs)   print_relocs(&f);
	if (do_syms)     print_syms(&f);
	if (do_notes)    print_notes(&f);
}
static int nfile_total;
static void dump_file(const char *path) {
	long size; u8 *data = slurp(path, &size);
	if (ar_is_archive(data, size)) {
		Archive ar; ar_open(&ar, path, data, size);
		long pos = 8; ArMember m;
		while (ar_next(&ar, &pos, &m)) {
			/* a THIN archive's member is named in square brackets (readelf); a real one's in parentheses */
			printf(ar.thin ? "\nFile: %s[%s]\n" : "\nFile: %s(%s)\n", path, m.name);
			if (m.data) {
				u8 *copy = malloc((size_t)m.size + 4); if (!copy) die("out of memory");
				memcpy(copy, m.data, (size_t)m.size);
				dump_elf(copy, m.size, path); free(copy);
			} else {
				char *p = ar_member_path(&ar, &m); long n; u8 *d = slurp(p, &n);
				dump_elf(d, n, p); free(d); free(p);
			}
		}
		free(data);
		return;
	}
	if (nfile_total > 1) printf("\nFile: %s\n", path);
	dump_elf(data, size, path);
	free(data);
}

static const char *usage =
	"Usage: readelf <option(s)> elf-file(s)\n"
	" Display information about the contents of ELF format files\n"
	" Options are:\n"
	"  -h --file-header       Display the ELF file header\n"
	"  -S --section-headers   Display the sections' header\n"
	"  -s --syms              Display the symbol table\n"
	"  -r --relocs            Display the relocations (if present)\n"
	"  -d --dynamic           Display the dynamic section (if present)\n"
	"  -n --notes             Display the core notes (if present)\n"
	"  -W --wide              Allow output width to exceed 80 characters\n"
	"  -H --help              Display this information\n";

int main(int argc, char **argv) {
	const char *files[256]; int nfile = 0;
	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];
		if (a[0] == '-' && a[1] && a[1] != '-') {         /* readelf takes clustered short options: -rW, -aW */
			for (const char *c = a + 1; *c; c++) switch (*c) {
			case 'h': do_header = 1; break;
			case 'S': do_sections = 1; break;
			case 's': do_syms = 1; break;
			case 'r': do_relocs = 1; break;
			case 'd': do_dynamic = 1; break;
			case 'n': do_notes = 1; break;
			case 'W': wide = 1; break;
			case 'a': die("option '-a' is not supported (it implies program headers, section groups, "
			              "version info and the bucket histogram — give -h -S -s -r -d -n)");
			              /* die() never returns */
			case 'H': fputs(usage, stdout); return 0;
			default: die("invalid option -- '%c'", *c);
			}
			continue;
		}
		if (!strcmp(a, "--file-header")) do_header = 1;
		else if (!strcmp(a, "--section-headers") || !strcmp(a, "--sections")) do_sections = 1;
		else if (!strcmp(a, "--syms") || !strcmp(a, "--symbols")) do_syms = 1;
		else if (!strcmp(a, "--relocs")) do_relocs = 1;
		else if (!strcmp(a, "--dynamic")) do_dynamic = 1;
		else if (!strcmp(a, "--notes")) do_notes = 1;
		else if (!strcmp(a, "--wide")) wide = 1;
		else if (!strcmp(a, "--all")) die("option '--all' is not supported (it implies program headers, section "
		                                  "groups, version info and the bucket histogram — give -h -S -s -r -d -n)");
		else if (!strcmp(a, "--help")) { fputs(usage, stdout); return 0; }
		else if (!strcmp(a, "--version")) { printf("readelf (os toolchain) 2.42\n"); return 0; }
		else if (a[0] == '-') die("unrecognized option '%s'", a);
		else { if (nfile == 256) die("too many files"); files[nfile++] = a; }
	}
	if (!nfile) { fputs(usage, stderr); die("Nothing to do."); }
	if (!do_header && !do_sections && !do_syms && !do_relocs && !do_dynamic && !do_notes)
		die("Nothing to do: give -h, -S, -s, -r, -d or -n");
	nfile_total = nfile;
	for (int i = 0; i < nfile; i++) dump_file(files[i]);
	return 0;
}
