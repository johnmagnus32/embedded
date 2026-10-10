/*
 * common/elfver.c — see elfver.h. The layouts: Elf32_Verdef { version, flags, ndx, cnt, hash, aux, next } is 20
 * bytes and its first Verdaux { name, next } holds the version's own name; Elf32_Verneed { version, cnt, file,
 * aux, next } is 16, and each Vernaux { hash, flags, other, name, next } names a needed version by `other`.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include "elfver.h"

#define SHT_GNU_verdef  0x6ffffffdu
#define SHT_GNU_verneed 0x6ffffffeu
#define SHT_GNU_versym  0x6fffffffu
#define VER_FLG_BASE     0x1

void versions_load(const ElfFile *f, Versions *v) {
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

const char *version_of(const Versions *v, int i, int *hidden) {
	if (!v->active) return NULL;
	u16 raw = i ? v->versym[i] : 0; *hidden = (raw & VERSYM_HIDDEN) != 0;
	unsigned n = raw & ~VERSYM_HIDDEN;
	if (!n) return "";
	if ((int)n <= v->nverdef && v->verdef && v->verdef[n]) return v->verdef[n];
	if ((int)n <= v->nverneed && v->verneed && v->verneed[n]) return v->verneed[n];
	return "<corrupt>";
}
