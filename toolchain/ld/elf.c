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

/* Write each allocatable PROGBITS section (already relocated in place) at its assigned file offset
 * vaddr - LOAD_BASE, zero-padding the gap first. Iterating RO then writable reproduces layout()'s order,
 * so the zero fill absorbs both per-section alignment and the page gap before the R-W segment. */
static void write_image(FILE *f, int want_write) {
	for (int i = 0; i < nobj; i++) for (int j = 0; j < objs[i].nsh; j++) {
		Elf32_Shdr *s = &objs[i].sh[j];
		if (!(s->sh_flags & SHF_ALLOC) || s->sh_type == SHT_NOBITS || !s->sh_size) continue;
		if (!!(s->sh_flags & SHF_WRITE) != want_write) continue;
		for (long p = ftell(f); p < (long)(objs[i].sec_vaddr[j] - LOAD_BASE); p++) fputc(0, f);
		fwrite(objs[i].data + s->sh_offset, 1, s->sh_size, f);
	}
}

/* Serialize the static executable: ehdr + program headers (R-X seg, optional R-W seg) + the loadable
 * image + a section-header table describing the real output sections (.text/.data/.bss/.shstrtab) so
 * readelf/objdump can inspect it. Program headers are what the kernel actually maps. */
void elf_write_exec(const char *out, u32 entry, const Layout *L) {
	FILE *f = fopen(out, "wb"); if (!f) die("cannot open %s", out);
	u32 hdrsz    = sizeof(Elf32_Ehdr) + 2 * sizeof(Elf32_Phdr);
	u32 bss_size = L->rw_memsz - L->rw_filesz;
	int have_rw = L->rw_memsz > 0, have_data = L->rw_filesz > 0, have_bss = bss_size > 0;

	/* .shstrtab: name offsets .text=1, .data=7, .bss=13, .shstrtab=18. */
	const char shstr[] = "\0.text\0.data\0.bss\0.shstrtab";

	/* Build the section headers we actually have: [0]=null, .text, [.data], [.bss], .shstrtab. */
	Elf32_Shdr sh[5] = {0}; int ns = 1;
	int ndx_text = ns++;
	sh[ndx_text] = (Elf32_Shdr){ .sh_name=1, .sh_type=SHT_PROGBITS, .sh_flags=SHF_ALLOC|SHF_EXECINSTR,
	    .sh_addr=LOAD_BASE+hdrsz, .sh_offset=hdrsz, .sh_size=L->rx_filesz-hdrsz, .sh_addralign=4 };
	if (have_data) sh[ns++] = (Elf32_Shdr){ .sh_name=7, .sh_type=SHT_PROGBITS, .sh_flags=SHF_ALLOC|SHF_WRITE,
	    .sh_addr=L->rw_vaddr, .sh_offset=L->rw_off, .sh_size=L->rw_filesz, .sh_addralign=4 };
	if (have_bss)  sh[ns++] = (Elf32_Shdr){ .sh_name=13, .sh_type=SHT_NOBITS, .sh_flags=SHF_ALLOC|SHF_WRITE,
	    .sh_addr=L->rw_vaddr+L->rw_filesz, .sh_offset=L->rw_off+L->rw_filesz, .sh_size=bss_size, .sh_addralign=4 };
	int ndx_shstr = ns++;
	u32 shstr_off = L->rw_off + L->rw_filesz;               /* .shstrtab follows the on-disk image */
	sh[ndx_shstr] = (Elf32_Shdr){ .sh_name=18, .sh_type=SHT_STRTAB, .sh_offset=shstr_off, .sh_size=sizeof(shstr), .sh_addralign=1 };
	u32 shoff = alignup(shstr_off + sizeof(shstr), 4);

	Elf32_Ehdr eh = {0};
	memcpy(eh.e_ident, "\177ELF\1\1\1", 7);          /* MAG + ELFCLASS32 + ELFDATA2LSB + EV_CURRENT */
	eh.e_type = ET_EXEC; eh.e_machine = md_e_machine; eh.e_version = 1; eh.e_entry = entry; eh.e_flags = 0x05000000;
	eh.e_phoff = sizeof(Elf32_Ehdr); eh.e_phentsize = sizeof(Elf32_Phdr); eh.e_phnum = have_rw ? 2 : 1;
	eh.e_ehsize = sizeof(Elf32_Ehdr); eh.e_shentsize = sizeof(Elf32_Shdr); eh.e_shnum = ns; eh.e_shstrndx = ndx_shstr; eh.e_shoff = shoff;

	/* p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, p_align */
	Elf32_Phdr ph[2] = {
		{ PT_LOAD, 0,         LOAD_BASE,   LOAD_BASE,   L->rx_filesz, L->rx_filesz, PF_R|PF_X, PAGE },
		{ PT_LOAD, L->rw_off, L->rw_vaddr, L->rw_vaddr, L->rw_filesz, L->rw_memsz,  PF_R|PF_W, PAGE },
	};

	fwrite(&eh, sizeof eh, 1, f);
	fwrite(ph, sizeof ph[0], have_rw ? 2 : 1, f);
	write_image(f, 0);                               /* R-X image: .text, .rodata          */
	write_image(f, 1);                               /* R-W image: .data (page gap auto-filled) */
	fwrite(shstr, 1, sizeof(shstr), f);
	for (long p = ftell(f); p < (long)shoff; p++) fputc(0, f);
	fwrite(sh, sizeof sh[0], ns, f);
	fclose(f);
}
