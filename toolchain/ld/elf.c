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

/* Read a shared library (-l) as a PROVIDER: register the symbols it EXPORTS + its soname, without laying
 * out any of its sections. We read its .dynsym (SHT_DYNSYM) + linked .dynstr for the global/weak DEFINED
 * names, and its DT_SONAME (falling back to the file's basename) for the DT_NEEDED we'll emit if used. */
void load_shared(const char *path) {
	long size; u8 *d = slurp(path, &size);
	Elf32_Ehdr *eh = (Elf32_Ehdr *)d;
	if (memcmp(eh->e_ident, "\177ELF\1\1", 6)) die("%s: not a little-endian ELF32", path);
	if (eh->e_type != ET_DYN) die("%s: not a shared object (e_type != ET_DYN)", path);
	Elf32_Shdr *sh = (Elf32_Shdr *)(d + eh->e_shoff);
	Elf32_Sym *dsym = NULL; int ndsym = 0; const char *dstr = NULL;
	Elf32_Dyn *dyn = NULL; int ndyn = 0;
	for (int i = 0; i < eh->e_shnum; i++) {
		if (sh[i].sh_type == SHT_DYNSYM) {
			dsym = (Elf32_Sym *)(d + sh[i].sh_offset); ndsym = sh[i].sh_size / sizeof(Elf32_Sym);
			dstr = (const char *)(d + sh[sh[i].sh_link].sh_offset);
		} else if (sh[i].sh_type == SHT_DYNAMIC) {
			dyn = (Elf32_Dyn *)(d + sh[i].sh_offset); ndyn = sh[i].sh_size / sizeof(Elf32_Dyn);
		}
	}
	if (!dsym || !dstr) die("%s: no .dynsym (not a linkable shared object)", path);

	const char *son = NULL;
	for (int i = 0; i < ndyn; i++) if (dyn[i].d_tag == DT_SONAME) son = dstr + dyn[i].d_val;
	if (!son) { const char *b = strrchr(path, '/'); son = strdup(b ? b + 1 : path); }
	if (nshlib >= MAXSHLIB) die("too many shared libraries");
	int lib = nshlib; shlibs[nshlib++] = (ShLib){ son, 0 };

	for (int k = 0; k < ndsym; k++) {
		Elf32_Sym *s = &dsym[k]; int b = ELF32_ST_BIND(s->st_info);
		if ((b == STB_GLOBAL || b == STB_WEAK) && s->st_shndx != SHN_UNDEF && s->st_name) {
			if (nshexport >= MAXSHEXPORT) die("too many shared-library exports");
			shexports[nshexport++] = (ShExport){ dstr + s->st_name, lib, s->st_size };
		}
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

static void put32(FILE *f, u32 v) { fwrite(&v, 4, 1, f); }   /* host is LE, like every struct write here */

/* SysV ELF hash (System V gABI) — MUST match the runtime loader's elf_hash (libc/ld/src/reloc.h). */
static u32 elf_hash(const char *s) {
	u32 h = 0, g;
	for (const u8 *p = (const u8 *)s; *p; p++) {
		h = (h << 4) + *p; g = h & 0xf0000000u; if (g) h ^= g >> 24; h &= ~g;
	}
	return h;
}

/* .dynstr offsets recorded by write_dynsyms for write_dynamic (DT_NEEDED per used lib, DT_SONAME). */
static u32 needed_stroff[MAXSHLIB]; static u32 soname_stroff;

/* Write the dynamic symbol tables (.hash/.dynsym/.dynstr) at the R-X-segment tail. The .dynsym is
 * [null] + IMPORTS (undefined refs into providers) + EXPORTS (this object's own defined globals, -shared);
 * the .hash chains them all (imports are UNDEF so the loader's dso_lookup skips them). As it lays out
 * .dynstr it also records the offsets of the provider sonames (DT_NEEDED) + our own soname (DT_SONAME). */
static void write_dynsyms(FILE *f, const Layout *L) {
	int nsym = nimport + nexport;                        /* non-null dynamic symbols */
	u32 nchain = (u32)nsym + 1, nbucket = pick_nbucket(nchain);

	for (long p = ftell(f); p < (long)L->hash_off; p++) fputc(0, f);
	u32 *bucket = calloc(nbucket, 4), *chain = calloc(nchain, 4);
	for (int y = 1; y <= nsym; y++) {
		const char *nm = (y <= nimport) ? imports[y-1].name : exports[y-1-nimport].name;
		u32 b = elf_hash(nm) % nbucket;
		chain[y] = bucket[b]; bucket[b] = (u32)y;        /* prepend: newest at the bucket head */
	}
	put32(f, nbucket); put32(f, nchain);
	for (u32 i = 0; i < nbucket; i++) put32(f, bucket[i]);
	for (u32 i = 0; i < nchain;  i++) put32(f, chain[i]);
	free(bucket); free(chain);

	for (long p = ftell(f); p < (long)L->dynsym_off; p++) fputc(0, f);
	Elf32_Sym z = {0}; fwrite(&z, sizeof z, 1, f);       /* index 0: STN_UNDEF */
	u32 off = 1;                                         /* running .dynstr offset (0 = leading '\0') */
	for (int i = 0; i < nimport; i++) {                  /* imports: UNDEF entries the relocs reference */
		Elf32_Sym d = { .st_name = off, .st_shndx = SHN_UNDEF,   /* DATA import carries its size -> R_ARM_COPY memcpy len */
		    .st_info = ELF32_ST_INFO(STB_GLOBAL, imports[i].is_data ? STT_OBJECT : STT_FUNC),
		    .st_size = imports[i].is_data ? imports[i].copy_size : 0 };
		fwrite(&d, sizeof d, 1, f);
		off += (u32)strlen(imports[i].name) + 1;
	}
	for (int i = 0; i < nexport; i++) {                  /* exports: defined, base-relative st_value */
		Elf32_Sym *s = &exports[i].obj->sym[exports[i].symidx];
		Elf32_Sym d = { .st_name = off,
		    .st_value = exports[i].obj->sec_vaddr[s->st_shndx] + s->st_value,
		    .st_size = s->st_size, .st_info = s->st_info, .st_other = s->st_other, .st_shndx = 1 /* !=UNDEF */ };
		fwrite(&d, sizeof d, 1, f);
		off += (u32)strlen(exports[i].name) + 1;
	}

	for (long p = ftell(f); p < (long)L->dynstr_off; p++) fputc(0, f);
	fputc(0, f); off = 1;                                /* keep in lockstep with dynstr_bytes()'s order */
	for (int i = 0; i < nimport; i++) { fwrite(imports[i].name, 1, strlen(imports[i].name) + 1, f); off += (u32)strlen(imports[i].name) + 1; }
	for (int i = 0; i < nexport; i++) { fwrite(exports[i].name, 1, strlen(exports[i].name) + 1, f); off += (u32)strlen(exports[i].name) + 1; }
	for (int i = 0; i < nshlib; i++) if (shlibs[i].used) {
		needed_stroff[i] = off; fwrite(shlibs[i].soname, 1, strlen(shlibs[i].soname) + 1, f); off += (u32)strlen(shlibs[i].soname) + 1;
	}
	if (shared) { soname_stroff = off; fwrite(soname, 1, strlen(soname) + 1, f); }
}

/* PT_INTERP string: the runtime loader path (dynamic consumer only). */
static void write_interp(FILE *f, const Layout *L) {
	for (long p = ftell(f); p < (long)L->interp_off; p++) fputc(0, f);
	fwrite(INTERP_PATH, 1, sizeof(INTERP_PATH), f);
}

/* The PLT: one eager-binding stub per called import. PC-RELATIVE so it works position-independently (a
 * .so is loaded at a bias): the stub adds a link-time-constant offset to pc to reach its GOT slot, then
 * jumps to *slot (filled by the loader's JUMP_SLOT reloc). The offset is a difference of two in-image
 * addresses, so it needs no relocation and never writes .text — W^X-clean in a .so, and correct in an
 * ET_EXEC too (base 0). Layout per stub (16 bytes): [ldr ip,[pc,#4]][add ip,pc,ip][ldr pc,[ip]][.word off]. */
static void write_plt(FILE *f, const Layout *L) {
	for (long p = ftell(f); p < (long)L->plt_off; p++) fputc(0, f);
	for (int k = 0; k < nplt; k++) {
		u32 stub = L->plt_vaddr + PLTENT * (u32)k;       /* this stub's vaddr */
		u32 slot = L->gotplt_vaddr + 4u * (u32)k;        /* its GOT slot */
		put32(f, 0xe59fc004u);                           /* ldr ip, [pc, #4]  -> ip = the .word below      */
		put32(f, 0xe08fc00cu);                           /* add ip, pc, ip    -> ip = &got_slot (pc-relative) */
		put32(f, 0xe59cf000u);                           /* ldr pc, [ip]      -> jump to *got_slot          */
		put32(f, slot - (stub + 12));                    /* .word got_slot - (pc at the `add`); no reloc    */
	}
}

/* The PLT relocation table (DT_JMPREL): one R_ARM_JUMP_SLOT per stub, telling the loader to write the
 * resolved function address into GOT slot k. Symbol index refers to the import's .dynsym entry. */
static void write_relplt(FILE *f, const Layout *L) {
	for (long p = ftell(f); p < (long)L->relplt_off; p++) fputc(0, f);
	for (int k = 0; k < nplt; k++)
		for (int i = 0; i < nimport; i++) if (imports[i].plt_index == k) {
			Elf32_Rel re = { L->gotplt_vaddr + 4u * (u32)k,
			                 ELF32_R_INFO((u32)imports[i].dynsym_index, md_r_jump_slot) };
			fwrite(&re, sizeof re, 1, f); break;
		}
}

/* The PLT's GOT (R-W): nplt zero words; the loader stores each resolved address here at startup. */
static void write_gotplt(FILE *f, const Layout *L) {
	for (long p = ftell(f); p < (long)L->gotplt_off; p++) fputc(0, f);
	for (int k = 0; k < nplt; k++) put32(f, 0);
}

/* The PIC GOT (R-W): one word per entry. A LOCAL slot holds the symbol's link-time address (its
 * R_ARM_RELATIVE adds the load bias); an IMPORT slot is 0 (its R_ARM_GLOB_DAT fills it at runtime). */
static void write_got(FILE *f, const Layout *L) {
	for (long p = ftell(f); p < (long)L->got_off; p++) fputc(0, f);
	for (int i = 0; i < ngotent; i++) put32(f, gotents[i].is_import ? 0 : gotents[i].symval);
}

/* Zero-pad to .rel.dyn, write the R_ARM_RELATIVE table (pie/shared only), then the .dynamic array. The
 * tag set varies: DT_NEEDED per used provider; DT_SONAME (-shared); the DT_HASH/STRTAB/SYMTAB/STRSZ/SYMENT
 * group when there's a .dynsym; the DT_PLTGOT/PLTRELSZ/PLTREL/JMPREL group when there's a PLT; the
 * DT_REL* group for base fixups. Emission order + count are kept in lockstep with count_dyn_entries(). */
static void write_dynamic(FILE *f, const Layout *L) {
	for (long p = ftell(f); p < (long)L->reldyn_off; p++) fputc(0, f);
	for (int i = 0; i < ndynrel; i++) {                              /* RELATIVE first (DT_RELCOUNT counts these) */
		Elf32_Rel re = { dynrel[i], ELF32_R_INFO(0, md_r_relative) };   /* sym 0: pure base fixup */
		fwrite(&re, sizeof re, 1, f);
	}
	for (int i = 0; i < nglobdat; i++) {                             /* GLOB_DAT: loader writes an import's addr into its GOT slot */
		Elf32_Rel re = { globdat[i].offset, ELF32_R_INFO(globdat[i].dynsym_index, md_r_glob_dat) };
		fwrite(&re, sizeof re, 1, f);
	}
	for (int i = 0; i < ncopyrel; i++) {                             /* then COPY: memcpy an imported var into .dynbss */
		Elf32_Rel re = { copyrel[i].offset, ELF32_R_INFO(copyrel[i].dynsym_index, md_r_copy) };
		fwrite(&re, sizeof re, 1, f);
	}
	for (long p = ftell(f); p < (long)L->dynamic_off; p++) fputc(0, f);
	Elf32_Dyn dyn[MAXSHLIB + 16]; int n = 0;
	for (int i = 0; i < nshlib; i++) if (shlibs[i].used) dyn[n++] = (Elf32_Dyn){ DT_NEEDED, needed_stroff[i] };
	if (shared) dyn[n++] = (Elf32_Dyn){ DT_SONAME, soname_stroff };
	if (shared || nimport) {
		dyn[n++] = (Elf32_Dyn){ DT_HASH,   L->hash_vaddr };
		dyn[n++] = (Elf32_Dyn){ DT_STRTAB, L->dynstr_vaddr };
		dyn[n++] = (Elf32_Dyn){ DT_SYMTAB, L->dynsym_vaddr };
		dyn[n++] = (Elf32_Dyn){ DT_STRSZ,  L->dynstr_sz };
		dyn[n++] = (Elf32_Dyn){ DT_SYMENT, sizeof(Elf32_Sym) };
	}
	if (nplt) {
		dyn[n++] = (Elf32_Dyn){ DT_PLTGOT,   L->gotplt_vaddr };
		dyn[n++] = (Elf32_Dyn){ DT_PLTRELSZ, L->relplt_sz };
		dyn[n++] = (Elf32_Dyn){ DT_PLTREL,   DT_REL };       /* our PLT relocs are Elf32_Rel */
		dyn[n++] = (Elf32_Dyn){ DT_JMPREL,   L->relplt_vaddr };
	}
	if (pie || shared || ncopyrel) {                     /* .rel.dyn: RELATIVE fixups and/or COPY relocs */
		dyn[n++] = (Elf32_Dyn){ DT_REL,      L->reldyn_vaddr };
		dyn[n++] = (Elf32_Dyn){ DT_RELSZ,    L->reldyn_sz };
		dyn[n++] = (Elf32_Dyn){ DT_RELENT,   sizeof(Elf32_Rel) };
		dyn[n++] = (Elf32_Dyn){ DT_RELCOUNT, (u32)ndynrel };   /* leading RELATIVE count (COPY relocs follow) */
	}
	dyn[n++] = (Elf32_Dyn){ DT_NULL, 0 };
	fwrite(dyn, sizeof dyn[0], n, f);                    /* n == L->dynamic_count */
}

/* Serialize the output image: ehdr + program headers (R-X seg, optional R-W seg, and for a PIE a
 * PT_DYNAMIC) + the loadable image + a section-header table describing the real output sections so
 * readelf/objdump can inspect it. Program headers are what the loader actually maps.
 *   ET_EXEC (default): fixed base, statically relocated, no .dynamic.
 *   ET_DYN  (-pie):    base 0; absolute refs left at their link-time value with an R_ARM_RELATIVE in
 *                      .rel.dyn so a tiny crt can add the load bias at startup. .rel.dyn + .dynamic sit
 *                      at the tail of the R-X segment; a PT_DYNAMIC header points at .dynamic. */
/* Write the ET_EXEC produced by a linker script: one PT_LOAD covering every placed section (bytes at
 * their script-assigned vaddr), with the ELF headers mapped just below the lowest section. Simple RWX
 * segment — bare-metal images don't need W^X, and they're usually objcopy'd to a raw binary anyway. */
void elf_write_script(const char *out, u32 entry) {
	struct { Obj *o; int j; u32 va, sz; int nobits; } seg[1024]; int nseg = 0;
	u32 lo = 0xffffffffu, hiprog = 0, himem = 0;
	for (int i = 0; i < nobj; i++) { if (!objs[i].active) continue; for (int j = 0; j < objs[i].nsh; j++) {
		Elf32_Shdr *s = &objs[i].sh[j];
		if (!(s->sh_flags & SHF_ALLOC) || !s->sh_size || !objs[i].sec_vaddr[j]) continue;   /* unplaced/discarded skipped */
		u32 va = objs[i].sec_vaddr[j];
		if (nseg < 1024) { seg[nseg].o=&objs[i]; seg[nseg].j=j; seg[nseg].va=va; seg[nseg].sz=s->sh_size; seg[nseg].nobits=(s->sh_type==SHT_NOBITS); nseg++; }
		if (va < lo) lo = va;
		if (va + s->sh_size > himem) himem = va + s->sh_size;
		if (s->sh_type != SHT_NOBITS && va + s->sh_size > hiprog) hiprog = va + s->sh_size;
	} }
	if (!nseg) die("linker script placed no sections");
	for (int a = 1; a < nseg; a++) for (int b = a; b > 0 && seg[b-1].va > seg[b].va; b--) {   /* insertion sort by vaddr */
		Obj *o=seg[b].o; int j=seg[b].j, nb=seg[b].nobits; u32 va=seg[b].va, sz=seg[b].sz;
		seg[b]=seg[b-1]; seg[b-1].o=o; seg[b-1].j=j; seg[b-1].nobits=nb; seg[b-1].va=va; seg[b-1].sz=sz;
	}

	u32 hdrsz = sizeof(Elf32_Ehdr) + sizeof(Elf32_Phdr);   /* one PT_LOAD */
	u32 pv = lo - hdrsz;                                   /* headers map just below the first section */

	/* .shstrtab: "\0" + each output-section name + ".shstrtab" */
	char shstr[2048]; int slen = 0; shstr[slen++] = 0;
	int nameoff[MAXOUTSEC];
	for (int i = 0; i < noutsec; i++) { nameoff[i]=slen; strcpy(shstr+slen, outsecs[i].name); slen += (int)strlen(outsecs[i].name)+1; }
	int shstr_nameoff = slen; strcpy(shstr+slen, ".shstrtab"); slen += 10;

	u32 content_end = hdrsz + (hiprog - lo);               /* headers + PROGBITS span */
	u32 shstr_off = content_end;
	u32 shoff = alignup(shstr_off + (u32)slen, 4);

	FILE *f = fopen(out, "wb"); if (!f) die("cannot open %s", out);
	Elf32_Ehdr eh = {0};
	memcpy(eh.e_ident, "\177ELF\1\1\1", 7);
	eh.e_type = ET_EXEC; eh.e_machine = md_e_machine; eh.e_version = 1; eh.e_entry = entry; eh.e_flags = 0x05000000;
	eh.e_phoff = sizeof(Elf32_Ehdr); eh.e_phentsize = sizeof(Elf32_Phdr); eh.e_phnum = 1;
	eh.e_ehsize = sizeof(Elf32_Ehdr); eh.e_shentsize = sizeof(Elf32_Shdr);
	eh.e_shnum = 1 + noutsec + 1; eh.e_shstrndx = noutsec + 1; eh.e_shoff = shoff;
	Elf32_Phdr ph = { PT_LOAD, 0, pv, pv, hdrsz + (hiprog - lo), hdrsz + (himem - lo), PF_R|PF_W|PF_X, PAGE };
	fwrite(&eh, sizeof eh, 1, f);
	fwrite(&ph, sizeof ph, 1, f);
	for (int i = 0; i < nseg; i++) {                       /* section bytes at (va - lo + hdrsz) */
		if (seg[i].nobits) continue;
		u32 off = seg[i].va - lo + hdrsz;
		for (long p = ftell(f); p < (long)off; p++) fputc(0, f);
		Elf32_Shdr *s = &seg[i].o->sh[seg[i].j];
		fwrite(seg[i].o->data + s->sh_offset, 1, s->sh_size, f);
	}
	for (long p = ftell(f); p < (long)content_end; p++) fputc(0, f);
	fwrite(shstr, 1, slen, f);
	for (long p = ftell(f); p < (long)shoff; p++) fputc(0, f);
	Elf32_Shdr sh = {0}; fwrite(&sh, sizeof sh, 1, f);     /* [0] null */
	for (int i = 0; i < noutsec; i++) {
		OutSec *os = &outsecs[i];
		Elf32_Shdr s = { .sh_name=nameoff[i], .sh_type=os->nobits?SHT_NOBITS:SHT_PROGBITS,
		    .sh_flags=SHF_ALLOC|(os->exec?SHF_EXECINSTR:0)|(os->write?SHF_WRITE:0),
		    .sh_addr=os->vaddr, .sh_offset=os->nobits?content_end:(os->vaddr-lo+hdrsz), .sh_size=os->size, .sh_addralign=4 };
		fwrite(&s, sizeof s, 1, f);
	}
	Elf32_Shdr ss = { .sh_name=shstr_nameoff, .sh_type=SHT_STRTAB, .sh_offset=shstr_off, .sh_size=(u32)slen, .sh_addralign=1 };
	fwrite(&ss, sizeof ss, 1, f);
	fclose(f);
}

void elf_write_exec(const char *out, u32 entry, const Layout *L) {
	FILE *f = fopen(out, "wb"); if (!f) die("cannot open %s", out);
	int need_dynamic = pie || shared || nimport || ngotent;           /* carries a .dynamic + PT_DYNAMIC */
	int need_dynsym  = shared || nimport;                             /* carries .hash/.dynsym/.dynstr */
	int has_interp   = nimport && !shared;                            /* PT_INTERP: a program only, never a .so */
	int nphdr    = 2 + (need_dynamic ? 1 : 0) + (has_interp ? 1 : 0); /* reserved slots (must match layout()) */
	u32 hdrsz    = sizeof(Elf32_Ehdr) + nphdr * sizeof(Elf32_Phdr);
	u32 bss_size = L->rw_memsz - L->rw_filesz - L->dynbss_sz;   /* real .bss; the .dynbss copy slots are separate */
	int have_rw = L->rw_memsz > 0, have_data = L->rw_filesz > 0, have_bss = bss_size > 0;

	const char shstr[] = "\0.text\0.interp\0.plt\0.hash\0.dynsym\0.dynstr\0.rel.plt\0.rel.dyn\0.dynamic\0.got.plt\0.got\0.data\0.bss\0.dynbss\0.shstrtab";
	enum { N_text=1, N_interp=7, N_plt=15, N_hash=20, N_dynsym=26, N_dynstr=34, N_relplt=42,
	       N_reldyn=51, N_dynamic=60, N_gotplt=69, N_got=78, N_data=83, N_bss=89, N_dynbss=94, N_shstr=102 };

	/* Section headers in file-offset order: [0]=null, .text, [.interp], [.plt], [.hash .dynsym .dynstr],
	 * [.rel.plt], [.rel.dyn], [.dynamic], [.got.plt], [.got], [.data], [.bss], [.dynbss], .shstrtab. Track
	 * dynsym/dynstr/gotplt indices for sh_link/sh_info. */
	Elf32_Shdr sh[20] = {0}; int ns = 1;
	int ndx_dynsym = 0, ndx_dynstr = 0, ndx_gotplt = 0;
	sh[ns++] = (Elf32_Shdr){ .sh_name=N_text, .sh_type=SHT_PROGBITS, .sh_flags=SHF_ALLOC|SHF_EXECINSTR,
	    .sh_addr=load_base+hdrsz, .sh_offset=hdrsz, .sh_size=L->text_size, .sh_addralign=4 };
	if (has_interp) sh[ns++] = (Elf32_Shdr){ .sh_name=N_interp, .sh_type=SHT_PROGBITS, .sh_flags=SHF_ALLOC,
	    .sh_addr=L->interp_vaddr, .sh_offset=L->interp_off, .sh_size=L->interp_sz, .sh_addralign=1 };
	if (nplt) sh[ns++] = (Elf32_Shdr){ .sh_name=N_plt, .sh_type=SHT_PROGBITS, .sh_flags=SHF_ALLOC|SHF_EXECINSTR,
	    .sh_addr=L->plt_vaddr, .sh_offset=L->plt_off, .sh_size=L->plt_sz, .sh_addralign=4 };
	if (need_dynsym) {
		int ndx_hash = ns;
		sh[ns++] = (Elf32_Shdr){ .sh_name=N_hash, .sh_type=SHT_HASH, .sh_flags=SHF_ALLOC,
		    .sh_addr=L->hash_vaddr, .sh_offset=L->hash_off, .sh_size=L->hash_sz, .sh_addralign=4, .sh_entsize=4 };
		ndx_dynsym = ns;
		sh[ns++] = (Elf32_Shdr){ .sh_name=N_dynsym, .sh_type=SHT_DYNSYM, .sh_flags=SHF_ALLOC,
		    .sh_addr=L->dynsym_vaddr, .sh_offset=L->dynsym_off, .sh_size=L->dynsym_sz, .sh_addralign=4,
		    .sh_entsize=sizeof(Elf32_Sym), .sh_info=1 /* first non-local symbol */ };
		ndx_dynstr = ns;
		sh[ns++] = (Elf32_Shdr){ .sh_name=N_dynstr, .sh_type=SHT_STRTAB, .sh_flags=SHF_ALLOC,
		    .sh_addr=L->dynstr_vaddr, .sh_offset=L->dynstr_off, .sh_size=L->dynstr_sz, .sh_addralign=1 };
		sh[ndx_hash].sh_link = ndx_dynsym;           /* .hash -> its symbol table */
		sh[ndx_dynsym].sh_link = ndx_dynstr;         /* .dynsym -> its string table */
	}
	if (nplt) {
		/* .got.plt's own section index isn't known until it's emitted below; fill sh_info afterward. */
		sh[ns++] = (Elf32_Shdr){ .sh_name=N_relplt, .sh_type=SHT_REL, .sh_flags=SHF_ALLOC,
		    .sh_addr=L->relplt_vaddr, .sh_offset=L->relplt_off, .sh_size=L->relplt_sz, .sh_addralign=4,
		    .sh_entsize=sizeof(Elf32_Rel), .sh_link=ndx_dynsym };
	}
	int ndx_relplt = nplt ? ns - 1 : 0;
	if (L->reldyn_sz) sh[ns++] = (Elf32_Shdr){ .sh_name=N_reldyn, .sh_type=SHT_REL, .sh_flags=SHF_ALLOC,
	    .sh_addr=L->reldyn_vaddr, .sh_offset=L->reldyn_off, .sh_size=L->reldyn_sz, .sh_addralign=4,
	    .sh_entsize=sizeof(Elf32_Rel), .sh_link=ndx_dynsym };
	if (need_dynamic) sh[ns++] = (Elf32_Shdr){ .sh_name=N_dynamic, .sh_type=SHT_DYNAMIC, .sh_flags=SHF_ALLOC,
	    .sh_addr=L->dynamic_vaddr, .sh_offset=L->dynamic_off, .sh_size=L->dynamic_sz, .sh_addralign=4,
	    .sh_entsize=sizeof(Elf32_Dyn), .sh_link=ndx_dynstr };
	if (nplt) {
		ndx_gotplt = ns;
		sh[ns++] = (Elf32_Shdr){ .sh_name=N_gotplt, .sh_type=SHT_PROGBITS, .sh_flags=SHF_ALLOC|SHF_WRITE,
		    .sh_addr=L->gotplt_vaddr, .sh_offset=L->gotplt_off, .sh_size=L->gotplt_sz, .sh_addralign=4 };
		sh[ndx_relplt].sh_info = ndx_gotplt;         /* .rel.plt modifies .got.plt */
	}
	if (L->got_sz) sh[ns++] = (Elf32_Shdr){ .sh_name=N_got, .sh_type=SHT_PROGBITS, .sh_flags=SHF_ALLOC|SHF_WRITE,
	    .sh_addr=L->got_vaddr, .sh_offset=L->got_off, .sh_size=L->got_sz, .sh_addralign=4 };
	u32 rw_prefix = L->gotplt_sz + L->got_sz;        /* synthetic RW head (.got.plt + .got) before .data */
	if (have_data && L->rw_filesz > rw_prefix) sh[ns++] = (Elf32_Shdr){ .sh_name=N_data, .sh_type=SHT_PROGBITS,
	    .sh_flags=SHF_ALLOC|SHF_WRITE, .sh_addr=L->rw_vaddr+rw_prefix, .sh_offset=L->rw_off+rw_prefix,
	    .sh_size=L->rw_filesz-rw_prefix, .sh_addralign=4 };
	if (have_bss)  sh[ns++] = (Elf32_Shdr){ .sh_name=N_bss, .sh_type=SHT_NOBITS, .sh_flags=SHF_ALLOC|SHF_WRITE,
	    .sh_addr=L->rw_vaddr+L->rw_filesz, .sh_offset=L->rw_off+L->rw_filesz, .sh_size=bss_size, .sh_addralign=4 };
	if (L->dynbss_sz) sh[ns++] = (Elf32_Shdr){ .sh_name=N_dynbss, .sh_type=SHT_NOBITS, .sh_flags=SHF_ALLOC|SHF_WRITE,
	    .sh_addr=L->dynbss_vaddr, .sh_offset=L->rw_off+L->rw_filesz, .sh_size=L->dynbss_sz, .sh_addralign=4 };
	int ndx_shstr = ns++;
	/* .shstrtab follows the last byte actually written: end of the R-W file content if any, else end of
	 * the R-X segment (a .bss-only or code-only program writes no R-W bytes, so nothing pads out to rw_off). */
	u32 shstr_off = have_data ? L->rw_off + L->rw_filesz : L->rx_filesz;
	sh[ndx_shstr] = (Elf32_Shdr){ .sh_name=N_shstr, .sh_type=SHT_STRTAB, .sh_offset=shstr_off, .sh_size=sizeof(shstr), .sh_addralign=1 };
	u32 shoff = alignup(shstr_off + sizeof(shstr), 4);

	Elf32_Ehdr eh = {0};
	memcpy(eh.e_ident, "\177ELF\1\1\1", 7);          /* MAG + ELFCLASS32 + ELFDATA2LSB + EV_CURRENT */
	eh.e_type = need_dynamic ? ET_DYN : ET_EXEC;
	if (nimport && !pie && !shared) eh.e_type = ET_EXEC;   /* a fixed-base program that imports from a .so */
	eh.e_machine = md_e_machine; eh.e_version = 1; eh.e_entry = entry; eh.e_flags = 0x05000000;
	eh.e_phoff = sizeof(Elf32_Ehdr); eh.e_phentsize = sizeof(Elf32_Phdr);
	eh.e_phnum = (have_rw ? 2 : 1) + (need_dynamic ? 1 : 0) + (has_interp ? 1 : 0);
	eh.e_ehsize = sizeof(Elf32_Ehdr); eh.e_shentsize = sizeof(Elf32_Shdr); eh.e_shnum = ns; eh.e_shstrndx = ndx_shstr; eh.e_shoff = shoff;

	/* p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, p_align */
	Elf32_Phdr ph[4]; int np = 0;
	if (has_interp) ph[np++] = (Elf32_Phdr){ PT_INTERP, L->interp_off, L->interp_vaddr, L->interp_vaddr, L->interp_sz, L->interp_sz, PF_R, 1 };
	ph[np++] = (Elf32_Phdr){ PT_LOAD, 0, load_base, load_base, L->rx_filesz, L->rx_filesz, PF_R|PF_X, PAGE };
	if (have_rw) ph[np++] = (Elf32_Phdr){ PT_LOAD, L->rw_off, L->rw_vaddr, L->rw_vaddr, L->rw_filesz, L->rw_memsz, PF_R|PF_W, PAGE };
	if (need_dynamic) ph[np++] = (Elf32_Phdr){ PT_DYNAMIC, L->dynamic_off, L->dynamic_vaddr, L->dynamic_vaddr, L->dynamic_sz, L->dynamic_sz, PF_R, 4 };

	fwrite(&eh, sizeof eh, 1, f);
	fwrite(ph, sizeof ph[0], np, f);
	write_image(f, 0);                               /* R-X image: .text, .rodata                   */
	if (has_interp)     write_interp(f, L);             /* R-X tail: .interp                            */
	if (nplt)        write_plt(f, L);                /* R-X tail: .plt stubs                         */
	if (need_dynsym) write_dynsyms(f, L);            /* R-X tail: .hash, .dynsym, .dynstr            */
	if (nplt)        write_relplt(f, L);             /* R-X tail: .rel.plt (JUMP_SLOTs)              */
	if (need_dynamic) write_dynamic(f, L);           /* R-X tail: .rel.dyn then .dynamic             */
	if (nplt)        write_gotplt(f, L);             /* R-W head: .got.plt (zero slots)              */
	if (ngotent)     write_got(f, L);                /* R-W head: .got (local link-addrs / import 0) */
	write_image(f, 1);                               /* R-W image: .data (page gap auto-filled)      */
	fwrite(shstr, 1, sizeof(shstr), f);
	for (long p = ftell(f); p < (long)shoff; p++) fputc(0, f);
	fwrite(sh, sizeof sh[0], ns, f);
	fclose(f);
}
