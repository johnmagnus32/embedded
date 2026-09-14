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

/* Parse an in-memory ELF32 relocatable into objs[] (locating its symbol + string tables). `active`
 * distinguishes always-linked command-line objects (1) from lazy archive members (0, pulled on demand). */
Obj *elf_parse(const char *path, u8 *data, long size, int active) {
	if (nobj >= MAXOBJ) die("too many objects");
	Obj *o = &objs[nobj++]; o->path = path; o->data = data; o->size = size; o->active = active;
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

/* Read a whole file into a fresh buffer. */
static u8 *slurp(const char *path, long *size) {
	FILE *f = fopen(path, "rb"); if (!f) die("cannot open %s", path);
	fseek(f, 0, SEEK_END); *size = ftell(f); fseek(f, 0, SEEK_SET);
	u8 *b = malloc(*size); if (fread(b, 1, *size, f) != (size_t)*size) die("%s: read failed", path);
	fclose(f); return b;
}

/* Parse one relocatable object FILE (always active). */
Obj *elf_load(const char *path) { long n; u8 *d = slurp(path, &n); return elf_parse(path, d, n, 1); }

/* Split a `!<arch>\n` archive into its object members, each parsed LAZY (active=0). Handles the GNU
 * variant our ar writes: skip the "/" symbol index and "//" long-name table; resolve "/off" member
 * names against "//". Every 60-byte header is followed by sh_size bytes padded to an even length. */
void ar_load(const char *path) {
	long size; u8 *d = slurp(path, &size);
	if (size < 8 || memcmp(d, "!<arch>\n", 8)) die("%s: not an archive", path);
	const char *longtab = NULL;
	long p = 8;
	while (p + 60 <= size) {
		char *h = (char *)(d + p);                    /* 60-byte header: name[16] .. size[48..58] fmag[58..60] */
		char namef[17]; memcpy(namef, h, 16); namef[16] = 0;
		char szf[11];   memcpy(szf, h + 48, 10); szf[10] = 0;
		long msize = strtol(szf, NULL, 10);
		u8 *mdata = d + p + 60;
		if (!memcmp(namef, "//", 2) && (namef[2] == ' ' || namef[2] == 0)) {
			longtab = (const char *)mdata;            /* extended long-name table */
		} else if (namef[0] != '/') {                 /* short name "name/": copy up to the / or space */
			char name[64]; int k = 0; while (k < 15 && namef[k] && namef[k] != '/' && namef[k] != ' ') { name[k] = namef[k]; k++; } name[k] = 0;
			elf_parse(strdup(name), mdata, msize, 0);
		} else if (namef[1] >= '0' && namef[1] <= '9') {   /* "/off": long name referencing // */
			const char *nm = longtab ? longtab + atoi(namef + 1) : "member";
			char name[64]; int k = 0; while (k < 63 && nm[k] && nm[k] != '/' && nm[k] != '\n') { name[k] = nm[k]; k++; } name[k] = 0;
			elf_parse(strdup(name), mdata, msize, 0);
		}   /* else namef == "/" : the symbol index — skip (we scan member symtabs directly) */
		p += 60 + msize + (msize & 1);                /* members are padded to even length */
	}
}

/* Write each allocatable PROGBITS section (already relocated in place) at its assigned file offset
 * vaddr - LOAD_BASE, zero-padding the gap first. Iterating RO then writable reproduces layout()'s order,
 * so the zero fill absorbs both per-section alignment and the page gap before the R-W segment. */
static void write_image(FILE *f, int want_write) {
	for (int i = 0; i < nobj; i++) { if (!objs[i].active) continue; for (int j = 0; j < objs[i].nsh; j++) {
		Elf32_Shdr *s = &objs[i].sh[j];
		if (!(s->sh_flags & SHF_ALLOC) || s->sh_type == SHT_NOBITS || !s->sh_size) continue;
		if (!!(s->sh_flags & SHF_WRITE) != want_write) continue;
		for (long p = ftell(f); p < (long)(objs[i].sec_vaddr[j] - load_base); p++) fputc(0, f);
		fwrite(objs[i].data + s->sh_offset, 1, s->sh_size, f);
	} }
}

/* Zero-pad the file to `off`, then write the .rel.dyn (one R_ARM_RELATIVE per collected vaddr) and
 * .dynamic (the DT_* array) that a self-relocating PIE carries at the tail of its R-X segment. */
static void write_dynamic(FILE *f, const Layout *L) {
	for (long p = ftell(f); p < (long)L->reldyn_off; p++) fputc(0, f);
	for (int i = 0; i < ndynrel; i++) {
		Elf32_Rel re = { dynrel[i], ELF32_R_INFO(0, md_r_relative) };   /* sym 0: pure base fixup */
		fwrite(&re, sizeof re, 1, f);
	}
	for (long p = ftell(f); p < (long)L->dynamic_off; p++) fputc(0, f);
	Elf32_Dyn dyn[NDYNENT] = {
		{ DT_REL, L->reldyn_vaddr }, { DT_RELSZ, L->reldyn_sz }, { DT_RELENT, sizeof(Elf32_Rel) },
		{ DT_RELCOUNT, (u32)ndynrel }, { DT_NULL, 0 },
	};
	fwrite(dyn, sizeof dyn[0], NDYNENT, f);
}

/* Serialize the output image: ehdr + program headers (R-X seg, optional R-W seg, and for a PIE a
 * PT_DYNAMIC) + the loadable image + a section-header table describing the real output sections so
 * readelf/objdump can inspect it. Program headers are what the loader actually maps.
 *   ET_EXEC (default): fixed base, statically relocated, no .dynamic.
 *   ET_DYN  (-pie):    base 0; absolute refs left at their link-time value with an R_ARM_RELATIVE in
 *                      .rel.dyn so a tiny crt can add the load bias at startup. .rel.dyn + .dynamic sit
 *                      at the tail of the R-X segment; a PT_DYNAMIC header points at .dynamic. */
void elf_write_exec(const char *out, u32 entry, const Layout *L) {
	FILE *f = fopen(out, "wb"); if (!f) die("cannot open %s", out);
	int nphdr    = pie ? 3 : 2;                                       /* reserved header slots (must match layout()) */
	u32 hdrsz    = sizeof(Elf32_Ehdr) + nphdr * sizeof(Elf32_Phdr);
	u32 bss_size = L->rw_memsz - L->rw_filesz;
	int have_rw = L->rw_memsz > 0, have_data = L->rw_filesz > 0, have_bss = bss_size > 0;

	const char shstr[] = "\0.text\0.rel.dyn\0.dynamic\0.data\0.bss\0.shstrtab";
	enum { N_text=1, N_reldyn=7, N_dynamic=16, N_data=25, N_bss=31, N_shstr=36 };

	/* Section headers we actually have: [0]=null, .text, [.rel.dyn, .dynamic], [.data], [.bss], .shstrtab. */
	Elf32_Shdr sh[7] = {0}; int ns = 1;
	sh[ns++] = (Elf32_Shdr){ .sh_name=N_text, .sh_type=SHT_PROGBITS, .sh_flags=SHF_ALLOC|SHF_EXECINSTR,
	    .sh_addr=load_base+hdrsz, .sh_offset=hdrsz, .sh_size=L->text_size, .sh_addralign=4 };
	if (pie && L->reldyn_sz) sh[ns++] = (Elf32_Shdr){ .sh_name=N_reldyn, .sh_type=SHT_REL, .sh_flags=SHF_ALLOC,
	    .sh_addr=L->reldyn_vaddr, .sh_offset=L->reldyn_off, .sh_size=L->reldyn_sz, .sh_addralign=4, .sh_entsize=sizeof(Elf32_Rel) };
	if (pie) sh[ns++] = (Elf32_Shdr){ .sh_name=N_dynamic, .sh_type=SHT_DYNAMIC, .sh_flags=SHF_ALLOC,
	    .sh_addr=L->dynamic_vaddr, .sh_offset=L->dynamic_off, .sh_size=L->dynamic_sz, .sh_addralign=4, .sh_entsize=sizeof(Elf32_Dyn) };
	if (have_data) sh[ns++] = (Elf32_Shdr){ .sh_name=N_data, .sh_type=SHT_PROGBITS, .sh_flags=SHF_ALLOC|SHF_WRITE,
	    .sh_addr=L->rw_vaddr, .sh_offset=L->rw_off, .sh_size=L->rw_filesz, .sh_addralign=4 };
	if (have_bss)  sh[ns++] = (Elf32_Shdr){ .sh_name=N_bss, .sh_type=SHT_NOBITS, .sh_flags=SHF_ALLOC|SHF_WRITE,
	    .sh_addr=L->rw_vaddr+L->rw_filesz, .sh_offset=L->rw_off+L->rw_filesz, .sh_size=bss_size, .sh_addralign=4 };
	int ndx_shstr = ns++;
	/* .shstrtab follows the last byte actually written: end of .data if there is any, else end of the
	 * R-X segment (a .bss-only or code-only program writes no R-W bytes, so nothing pads out to rw_off). */
	u32 shstr_off = have_data ? L->rw_off + L->rw_filesz : L->rx_filesz;
	sh[ndx_shstr] = (Elf32_Shdr){ .sh_name=N_shstr, .sh_type=SHT_STRTAB, .sh_offset=shstr_off, .sh_size=sizeof(shstr), .sh_addralign=1 };
	u32 shoff = alignup(shstr_off + sizeof(shstr), 4);

	Elf32_Ehdr eh = {0};
	memcpy(eh.e_ident, "\177ELF\1\1\1", 7);          /* MAG + ELFCLASS32 + ELFDATA2LSB + EV_CURRENT */
	eh.e_type = pie ? ET_DYN : ET_EXEC; eh.e_machine = md_e_machine; eh.e_version = 1; eh.e_entry = entry; eh.e_flags = 0x05000000;
	eh.e_phoff = sizeof(Elf32_Ehdr); eh.e_phentsize = sizeof(Elf32_Phdr);
	eh.e_phnum = (have_rw ? 2 : 1) + (pie ? 1 : 0);
	eh.e_ehsize = sizeof(Elf32_Ehdr); eh.e_shentsize = sizeof(Elf32_Shdr); eh.e_shnum = ns; eh.e_shstrndx = ndx_shstr; eh.e_shoff = shoff;

	/* p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, p_align */
	Elf32_Phdr ph[3]; int np = 0;
	ph[np++] = (Elf32_Phdr){ PT_LOAD, 0, load_base, load_base, L->rx_filesz, L->rx_filesz, PF_R|PF_X, PAGE };
	if (have_rw) ph[np++] = (Elf32_Phdr){ PT_LOAD, L->rw_off, L->rw_vaddr, L->rw_vaddr, L->rw_filesz, L->rw_memsz, PF_R|PF_W, PAGE };
	if (pie)     ph[np++] = (Elf32_Phdr){ PT_DYNAMIC, L->dynamic_off, L->dynamic_vaddr, L->dynamic_vaddr, L->dynamic_sz, L->dynamic_sz, PF_R, 4 };

	fwrite(&eh, sizeof eh, 1, f);
	fwrite(ph, sizeof ph[0], np, f);
	write_image(f, 0);                               /* R-X image: .text, .rodata          */
	if (pie) write_dynamic(f, L);                    /* R-X tail: .rel.dyn then .dynamic   */
	write_image(f, 1);                               /* R-W image: .data (page gap auto-filled) */
	fwrite(shstr, 1, sizeof(shstr), f);
	for (long p = ftell(f); p < (long)shoff; p++) fputc(0, f);
	fwrite(sh, sizeof sh[0], ns, f);
	fclose(f);
}
