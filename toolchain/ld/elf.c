/*
 * elf.c — the OBJECT-FORMAT backend: read ELF32 relocatable inputs into the Obj model, and serialize
 * the output ET_EXEC. Everything that knows the on-disk ELF layout lives here; the front-end operates
 * on the parsed Obj, and the arch backend never touches the format. (A different object format —
 * COFF/Mach-O — would be a different file with the same two entry points.)
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ld.h"

/* Parse one relocatable object into objs[] (locating its symbol + string tables). */
Obj *elf_load(const char *path) {
	if (nobj >= MAXOBJ) die("too many objects");
	Obj *o = &objs[nobj++]; o->path = path;
	FILE *f = fopen(path, "rb"); if (!f) die("cannot open %s", path);
	fseek(f, 0, SEEK_END); o->size = ftell(f); fseek(f, 0, SEEK_SET);
	o->data = malloc(o->size);
	if (fread(o->data, 1, o->size, f) != (size_t)o->size) die("%s: read failed", path);
	fclose(f);

	o->eh = (Elf32_Ehdr *)o->data;
	if (memcmp(o->eh->e_ident, "\177ELF\1\1", 6)) die("%s: not a little-endian ELF32", path);
	if (o->eh->e_machine != md_e_machine) die("%s: wrong machine (e_machine=%u, want %u)", path, o->eh->e_machine, md_e_machine);
	o->sh = (Elf32_Shdr *)(o->data + o->eh->e_shoff); o->nsh = o->eh->e_shnum;
	o->sec_vaddr = calloc(o->nsh, sizeof(u32));
	for (int i = 0; i < o->nsh; i++) if (o->sh[i].sh_type == SHT_SYMTAB) {
		o->sym = (Elf32_Sym *)(o->data + o->sh[i].sh_offset);
		o->nsym = o->sh[i].sh_size / sizeof(Elf32_Sym);
		o->strtab = (const char *)(o->data + o->sh[o->sh[i].sh_link].sh_offset);
	}
	return o;
}

/* Write the static executable: ehdr + one PT_LOAD + the loadable image + a minimal section-header
 * table ([null] .text .shstrtab) so readelf/objdump can inspect it. The image is the allocated PROGBITS
 * sections (already relocated in place by the front-end), written in the layout order elf_load saw. */
void elf_write_exec(const char *out, u32 entry, u32 filesz_end, u32 memsz_end) {
	FILE *f = fopen(out, "wb"); if (!f) die("cannot open %s", out);
	u32 hdrsz = sizeof(Elf32_Ehdr) + sizeof(Elf32_Phdr);
	u32 text_size = filesz_end - (LOAD_BASE + hdrsz);
	const char shstr[] = "\0.text\0.shstrtab";       /* name offsets: .text=1, .shstrtab=7 */
	u32 shstr_off = hdrsz + text_size;
	u32 shoff = alignup(shstr_off + sizeof(shstr), 4);

	Elf32_Ehdr eh = {0};
	memcpy(eh.e_ident, "\177ELF\1\1\1", 7);          /* MAG + ELFCLASS32 + ELFDATA2LSB + EV_CURRENT */
	eh.e_type = ET_EXEC; eh.e_machine = md_e_machine; eh.e_version = 1; eh.e_entry = entry; eh.e_flags = 0x05000000;
	eh.e_phoff = sizeof(Elf32_Ehdr); eh.e_phentsize = sizeof(Elf32_Phdr); eh.e_phnum = 1;
	eh.e_ehsize = sizeof(Elf32_Ehdr); eh.e_shentsize = sizeof(Elf32_Shdr); eh.e_shnum = 3; eh.e_shstrndx = 2; eh.e_shoff = shoff;

	Elf32_Phdr ph = { PT_LOAD, 0, LOAD_BASE, LOAD_BASE, filesz_end - LOAD_BASE, memsz_end - LOAD_BASE, PF_R|PF_W|PF_X, 0x1000 };

	fwrite(&eh, sizeof eh, 1, f);
	fwrite(&ph, sizeof ph, 1, f);
	for (int i = 0; i < nobj; i++) for (int j = 0; j < objs[i].nsh; j++) {   /* the loadable image */
		Elf32_Shdr *s = &objs[i].sh[j];
		if ((s->sh_flags & SHF_ALLOC) && s->sh_type != SHT_NOBITS && s->sh_size)
			fwrite(objs[i].data + s->sh_offset, 1, s->sh_size, f);
	}
	fwrite(shstr, 1, sizeof(shstr), f);
	for (long p = ftell(f); p < (long)shoff; p++) fputc(0, f);

	Elf32_Shdr sh[3] = {0};
	sh[1] = (Elf32_Shdr){ .sh_name=1, .sh_type=SHT_PROGBITS, .sh_flags=SHF_ALLOC|SHF_EXECINSTR,
	                      .sh_addr=LOAD_BASE+hdrsz, .sh_offset=hdrsz, .sh_size=text_size, .sh_addralign=4 };
	sh[2] = (Elf32_Shdr){ .sh_name=7, .sh_type=SHT_STRTAB, .sh_offset=shstr_off, .sh_size=sizeof(shstr), .sh_addralign=1 };
	fwrite(sh, sizeof sh, 1, f);
	fclose(f);
}
