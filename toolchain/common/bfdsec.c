/* bfdsec.c — see bfdsec.h. The rules are BFD's elf.c (_bfd_elf_slurp_reloc_table's owner test and
 * bfd_section_from_shdr's handling of SYMTAB/STRTAB), reproduced so our tools list what GNU's list. */
#include "bfdsec.h"

int rel_is_normal_section(const ElfFile *f, int i) {
	const Elf32_Shdr *h = &f->sh[i];
	int exec = f->eh->e_type == ET_EXEC || f->eh->e_type == ET_DYN;
	if (exec && (h->sh_flags & SHF_ALLOC)) return 1;      /* a dynamic reloc section is an ordinary section */
	if (h->sh_flags & 0x800u /* SHF_COMPRESSED */) return 1;
	if (!h->sh_link || (int)h->sh_link != f->symtab) return 1;   /* not against THE symtab: not relocations */
	if (!h->sh_info || (int)h->sh_info >= f->nsh) return 1;
	u32 t = f->sh[h->sh_info].sh_type;
	return t == SHT_REL || t == SHT_RELA;                 /* relocations for relocations: not consumed */
}

int bfd_has_section(const ElfFile *f, int i) {
	const Elf32_Shdr *h = &f->sh[i];
	if (h->sh_type == SHT_REL || h->sh_type == SHT_RELA) return rel_is_normal_section(f, i);
	if (h->sh_type == SHT_NULL || h->sh_type == SHT_SYMTAB || h->sh_type == 18 /* SHT_SYMTAB_SHNDX */) return 0;
	if (h->sh_type == SHT_STRTAB) {
		if (i == f->eh->e_shstrndx) return 0;
		for (int j = 1; j < f->nsh; j++) if (f->sh[j].sh_type == SHT_SYMTAB && (int)f->sh[j].sh_link == i) return 0;
	}
	return 1;
}
