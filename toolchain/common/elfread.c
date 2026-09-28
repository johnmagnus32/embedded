/* common/elfread.c — see elfread.h. */
#include <string.h>
#include "elfread.h"

#if !defined(__BYTE_ORDER__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "the toolchain reads ELF32 little-endian structures in place: build it on a little-endian host"
#endif

static int in_file(const ElfFile *f, u32 off, u32 len) { return off <= (u32)f->size && len <= (u32)f->size - off; }

/* A string table: in the file, non-empty, NUL-terminated (so every in-range offset names a C string). */
static void check_strtab(const ElfFile *f, int i, const char *what) {
	if (i <= 0 || i >= f->nsh || f->sh[i].sh_type != SHT_STRTAB) die("%s: %s is not a string table (section %d)", f->path, what, i);
	Elf32_Shdr *s = &f->sh[i];
	if (!s->sh_size || f->data[s->sh_offset + s->sh_size - 1]) die("%s: %s is not NUL-terminated", f->path, what);
}

const char *elf_string(const ElfFile *f, int strsec, u32 off) {
	if (off >= f->sh[strsec].sh_size) die("%s: string offset %#x outside its table", f->path, off);
	return (const char *)f->data + f->sh[strsec].sh_offset + off;
}

void elf_open(ElfFile *f, const char *path, u8 *data, long size, u16 type, u16 machine) {
	memset(f, 0, sizeof *f);
	f->path = path; f->data = data; f->size = size;
	if ((unsigned long)data & 3) die("%s: internal: unaligned ELF image", path);
	if (size < (long)sizeof(Elf32_Ehdr) || memcmp(data, "\177ELF", 4)) die("%s: not an ELF file", path);
	if (data[4] != 1 || data[5] != 1) die("%s: not a little-endian ELF32 file", path);
	Elf32_Ehdr *eh = f->eh = (Elf32_Ehdr *)data;
	if (eh->e_type != type) die("%s: wrong ELF type %u (want %u)", path, eh->e_type, type);
	if (machine && eh->e_machine != machine) die("%s: wrong machine (e_machine=%u, want %u)", path, eh->e_machine, machine);
	if (!eh->e_shnum) die("%s: no section headers", path);
	if (eh->e_shentsize != sizeof(Elf32_Shdr) || eh->e_shoff & 3 || !in_file(f, eh->e_shoff, (u32)eh->e_shnum * sizeof(Elf32_Shdr)))
		die("%s: section header table outside the file", path);
	f->sh = (Elf32_Shdr *)(data + eh->e_shoff); f->nsh = eh->e_shnum;
	for (int i = 1; i < f->nsh; i++) {
		Elf32_Shdr *s = &f->sh[i];
		if (s->sh_type != SHT_NOBITS && !in_file(f, s->sh_offset, s->sh_size)) die("%s: section %d outside the file", path, i);
		if (s->sh_link >= (u32)f->nsh) die("%s: section %d links to missing section %u", path, i, s->sh_link);
	}
	check_strtab(f, eh->e_shstrndx, "the section-name table");
	f->shstr = (const char *)data + f->sh[eh->e_shstrndx].sh_offset;
	for (int i = 0; i < f->nsh; i++) elf_string(f, eh->e_shstrndx, f->sh[i].sh_name);

	for (int i = 1; i < f->nsh; i++) {                   /* the symbol table (a relocatable has at most one) */
		Elf32_Shdr *s = &f->sh[i];
		if (s->sh_type != SHT_SYMTAB && s->sh_type != SHT_DYNSYM) continue;
		if (s->sh_offset & 3 || s->sh_size % sizeof(Elf32_Sym)) die("%s: malformed symbol table (section %d)", path, i);
		check_strtab(f, (int)s->sh_link, "a symbol string table");
		Elf32_Sym *sym = (Elf32_Sym *)(data + s->sh_offset); int n = (int)(s->sh_size / sizeof(Elf32_Sym));
		for (int k = 0; k < n; k++) {
			elf_string(f, (int)s->sh_link, sym[k].st_name);
			u16 x = sym[k].st_shndx;
			if (x != SHN_UNDEF && x != SHN_ABS && x != SHN_COMMON && x >= f->nsh) die("%s: symbol %d in missing section %u", path, k, x);
		}
		if (s->sh_type == SHT_SYMTAB) {
			if (f->symtab) die("%s: more than one symbol table", path);
			f->symtab = i; f->sym = sym; f->nsym = n;
			f->strtab = (const char *)data + f->sh[s->sh_link].sh_offset;
		}
	}
	for (int i = 1; i < f->nsh && type == ET_REL; i++) {   /* relocations: into a real section, against a real symbol */
		Elf32_Shdr *s = &f->sh[i];
		if (s->sh_type != SHT_REL) continue;
		if (s->sh_offset & 3 || s->sh_size % sizeof(Elf32_Rel)) die("%s: malformed relocation section %d", path, i);
		if (!s->sh_info || s->sh_info >= (u32)f->nsh) die("%s: relocation section %d targets missing section %u", path, i, s->sh_info);
		if ((int)s->sh_link != f->symtab || !f->symtab) die("%s: relocation section %d is not against the symbol table", path, i);
		Elf32_Rel *r = (Elf32_Rel *)(data + s->sh_offset);
		for (u32 k = 0; k < s->sh_size / sizeof *r; k++)
			if (ELF32_R_SYM(r[k].r_info) >= (u32)f->nsym) die("%s: relocation %u of section %d names missing symbol %u", path, k, i, ELF32_R_SYM(r[k].r_info));
	}
}
