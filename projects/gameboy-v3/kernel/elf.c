/*
 * elf.c — ARM ELF loader (ELFCLASS32/ELFDATA2LSB), ET_EXEC and ET_DYN.
 *
 * Offsets verified vs man7 elf.5 + Linux fs/binfmt_elf.c. For each PT_LOAD:
 * allocate zeroed 4 KB frames, map them at p_vaddr+bias in the target address
 * space, copy p_filesz bytes from the file; the trailing (p_memsz - p_filesz) is
 * bss — already zero because pmm hands out zeroed frames.
 *
 * ET_EXEC loads at its fixed p_vaddr (bias must be 0). ET_DYN (PIE executables
 * AND the dynamic linker ld.so) links at vaddr 0 and is loaded at a caller-chosen
 * `bias`; entry and phdr addresses are biased too. The kernel performs NO
 * relocations — for a dynamic executable, ld.so (named by PT_INTERP) relocates
 * everything itself. See gv3-dynlink-abi notes.
 */

#include <stdint.h>
#include "elf.h"
#include "pmm.h"
#include "vm.h"
#include "libk.h"

int printf(const char *fmt, ...);

/* little-endian readers from a byte buffer */
static uint16_t rd16(const uint8_t *p) { return p[0] | (p[1] << 8); }
static uint32_t rd32(const uint8_t *p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Elf32_Ehdr / Elf32_Phdr field offsets (verified). */
#define E_TYPE      16   /* u16, ET_EXEC=2, ET_DYN=3 */
#define E_MACHINE   18   /* u16, EM_ARM=40 */
#define E_ENTRY     24   /* u32 */
#define E_PHOFF     28   /* u32 */
#define E_PHENTSIZE 42   /* u16 (=32) */
#define E_PHNUM     44   /* u16 */

#define P_TYPE      0    /* u32 */
#define P_OFFSET    4    /* u32 */
#define P_VADDR     8    /* u32 */
#define P_FILESZ    16   /* u32 */
#define P_MEMSZ     20   /* u32 */
#define P_FLAGS     24   /* u32 */

#define PT_LOAD     1
#define PT_INTERP   3
#define ET_EXEC     2
#define ET_DYN      3

#define PAGE_MASK   (PAGE_SIZE - 1)

int elf_load(uint32_t l1_pa, const void *img, uint32_t sz, uint32_t bias,
             struct elf_info *out)
{
	const uint8_t *e = (const uint8_t *)img;
	if (sz < 52) { printf("elf: too small\n"); return -1; }
	if (!(e[0] == 0x7F && e[1] == 'E' && e[2] == 'L' && e[3] == 'F')) {
		printf("elf: bad magic\n"); return -2;
	}
	if (e[4] != 1 || e[5] != 1) { printf("elf: not 32-bit LE\n"); return -3; }
	uint16_t etype = rd16(e + E_TYPE);
	if (etype != ET_EXEC && etype != ET_DYN) { printf("elf: not EXEC/DYN\n"); return -4; }
	if (rd16(e + E_MACHINE) != 40) { printf("elf: not EM_ARM\n"); return -5; }
	/* ET_EXEC has fixed addresses — a nonzero bias would be wrong. */
	if (etype == ET_EXEC && bias != 0) { printf("elf: ET_EXEC + bias\n"); return -8; }

	uint32_t phoff = rd32(e + E_PHOFF);
	uint16_t phent = rd16(e + E_PHENTSIZE);
	uint16_t phnum = rd16(e + E_PHNUM);
	uint32_t brk_end = 0;                    /* highest page-rounded PT_LOAD end */
	uint32_t phdr_va = 0;                    /* AT_PHDR: in-memory VA of the phdrs */

	memset(out, 0, sizeof(*out));

	/* The header fields are untrusted (execve loads arbitrary files). Bound the
	 * program-header table so `e + phoff + i*phent` stays inside the image, using
	 * overflow-safe comparisons (all operands are u32; a + b can wrap). */
	if (phent < 32 || phoff > sz || phnum > (sz - phoff) / phent) {
		printf("elf: bad phdr table\n"); return -10;
	}

	for (uint16_t i = 0; i < phnum; i++) {
		const uint8_t *ph = e + phoff + (uint32_t)i * phent;
		uint32_t ptype = rd32(ph + P_TYPE);

		if (ptype == PT_INTERP) {
			/* interpreter path string lives in the file at [off, off+filesz).
			 * Overflow-safe bounds: ioff<=sz and ilen<=sz-ioff (never ioff+ilen). */
			uint32_t ioff = rd32(ph + P_OFFSET);
			uint32_t ilen = rd32(ph + P_FILESZ);
			if (ilen == 0 || ilen > sizeof(out->interp) ||
			    ioff > sz || ilen > sz - ioff) {
				printf("elf: bad PT_INTERP\n"); return -9;
			}
			memcpy(out->interp, e + ioff, ilen);
			out->interp[ilen - 1] = '\0';   /* ilen already in [1, sizeof interp] */
			out->has_interp = 1;
			continue;
		}
		if (ptype != PT_LOAD)
			continue;

		uint32_t off    = rd32(ph + P_OFFSET);
		uint32_t vraw   = rd32(ph + P_VADDR);
		uint32_t vaddr  = vraw + bias;
		uint32_t filesz = rd32(ph + P_FILESZ);
		uint32_t memsz  = rd32(ph + P_MEMSZ);

		/* Untrusted fields — reject a segment whose file range leaves the image,
		 * or whose virtual range overflows / escapes the user address space.
		 * (Overflow-safe: no `off+filesz` or `vaddr+memsz` sums before checking.) */
		if (off > sz || filesz > sz - off) {
			printf("elf: PT_LOAD file range OOB\n"); return -11;
		}
		if (filesz > memsz ||                          /* file part can't exceed mem */
		    vraw + bias < bias ||                       /* bias add overflowed */
		    memsz > USER_VA_MAX - vaddr) {              /* [vaddr,vaddr+memsz) escapes */
			printf("elf: PT_LOAD vaddr range bad\n"); return -12;
		}

		/* AT_PHDR: if this PT_LOAD covers e_phoff, the phdrs are resident here at
		 * vaddr + (e_phoff - p_offset). (Segment-relative, not base+e_phoff.) */
		if (phoff >= off && phoff < off + filesz)
			phdr_va = vaddr + (phoff - off);

		/* Page range covering [vaddr, vaddr+memsz). Handle a non-page-aligned
		 * vaddr by mapping the containing page and copying at the in-page offset
		 * (p_offset and p_vaddr are congruent mod page size). */
		uint32_t vstart = vaddr & ~PAGE_MASK;
		uint32_t vend   = (vaddr + memsz + PAGE_MASK) & ~PAGE_MASK;

		for (uint32_t va = vstart; va < vend; va += PAGE_SIZE) {
			uint32_t pa = vm_walk(l1_pa, va);
			if (!pa) {                      /* not already mapped -> new frame */
				pa = pmm_alloc_page();      /* zeroed (bss handled for free)   */
				if (!pa) { printf("elf: OOM\n"); return -6; }
				if (vm_map_page(l1_pa, va, pa) != 0) {
					printf("elf: map fail @0x%x\n", va); return -7;
				}
			}
		}

		/* Copy file bytes into the mapped pages (byte-wise via the identity map
		 * of the freshly allocated physical frames). pa is guaranteed mapped by
		 * the loop above, but guard defensively — a NULL deref here would be a
		 * kernel-mode write to physical 0. */
		for (uint32_t j = 0; j < filesz; j++) {
			uint32_t va = vaddr + j;
			uint32_t pa = vm_walk(l1_pa, va);       /* -> physical byte addr */
			if (!pa) { printf("elf: unmapped copy @0x%x\n", va); return -7; }
			*(volatile uint8_t *)(uintptr_t)pa = e[off + j];
		}
		/* [vaddr+filesz, vaddr+memsz) stays zero (bss) — frames were zeroed. */
		if (vend > brk_end) brk_end = vend;     /* track the top for the heap */
	}

	out->entry      = rd32(e + E_ENTRY) + bias;
	out->brk_end    = brk_end;
	out->phdr_va    = phdr_va;
	out->phent      = phent;
	out->phnum      = phnum;
	out->is_dyn     = (etype == ET_DYN);
	return 0;
}
