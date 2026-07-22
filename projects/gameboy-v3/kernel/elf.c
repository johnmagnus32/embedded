/*
 * elf.c — minimal static-ET_EXEC loader for ARM (ELFCLASS32/ELFDATA2LSB).
 *
 * Offsets verified vs man7 elf.5 + Linux fs/binfmt_elf.c. ET_EXEC loads at its
 * fixed p_vaddr (no relocation, no PIE bias, no interpreter). For each PT_LOAD:
 * allocate zeroed 4 KB frames, map them at p_vaddr in the target address space,
 * copy p_filesz bytes from the file, and the trailing (p_memsz - p_filesz) is
 * bss — already zero because pmm hands out zeroed frames.
 */

#include <stdint.h>
#include "elf.h"
#include "pmm.h"
#include "vm.h"

int printf(const char *fmt, ...);

/* little-endian readers from a byte buffer */
static uint16_t rd16(const uint8_t *p) { return p[0] | (p[1] << 8); }
static uint32_t rd32(const uint8_t *p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Elf32_Ehdr / Elf32_Phdr field offsets (verified). */
#define E_TYPE      16   /* u16, ET_EXEC=2 */
#define E_MACHINE   18   /* u16, EM_ARM=40 */
#define E_ENTRY     24   /* u32 */
#define E_PHOFF     28   /* u32 */
#define E_PHENTSIZE 42   /* u16 (=32) */
#define E_PHNUM     44   /* u16 */

#define P_TYPE      0    /* u32, PT_LOAD=1 */
#define P_OFFSET    4    /* u32 */
#define P_VADDR     8    /* u32 */
#define P_FILESZ    16   /* u32 */
#define P_MEMSZ     20   /* u32 */
#define P_FLAGS     24   /* u32 */

#define PT_LOAD     1

#define PAGE_MASK   (PAGE_SIZE - 1)

int elf_load(uint32_t l1_pa, const void *img, uint32_t sz,
             uint32_t *entry_out, uint32_t *brk_out)
{
	const uint8_t *e = (const uint8_t *)img;
	if (sz < 52) { printf("elf: too small\n"); return -1; }
	if (!(e[0] == 0x7F && e[1] == 'E' && e[2] == 'L' && e[3] == 'F')) {
		printf("elf: bad magic\n"); return -2;
	}
	if (e[4] != 1 || e[5] != 1) { printf("elf: not 32-bit LE\n"); return -3; }
	if (rd16(e + E_TYPE) != 2)  { printf("elf: not ET_EXEC\n"); return -4; }
	if (rd16(e + E_MACHINE) != 40) { printf("elf: not EM_ARM\n"); return -5; }

	uint32_t phoff = rd32(e + E_PHOFF);
	uint16_t phent = rd16(e + E_PHENTSIZE);
	uint16_t phnum = rd16(e + E_PHNUM);
	uint32_t brk_end = 0;                    /* highest page-rounded PT_LOAD end */

	for (uint16_t i = 0; i < phnum; i++) {
		const uint8_t *ph = e + phoff + (uint32_t)i * phent;
		if (rd32(ph + P_TYPE) != PT_LOAD)
			continue;

		uint32_t off    = rd32(ph + P_OFFSET);
		uint32_t vaddr  = rd32(ph + P_VADDR);
		uint32_t filesz = rd32(ph + P_FILESZ);
		uint32_t memsz  = rd32(ph + P_MEMSZ);

		/* Page range covering [vaddr, vaddr+memsz). Handle a non-page-aligned
		 * vaddr by mapping the containing page and copying at the in-page
		 * offset (p_offset and p_vaddr are congruent mod page size). */
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

		/* Copy file bytes into the mapped pages (byte-wise via the identity
		 * map of the freshly allocated physical frames). */
		for (uint32_t j = 0; j < filesz; j++) {
			uint32_t va = vaddr + j;
			uint32_t pa = vm_walk(l1_pa, va);       /* -> physical byte addr */
			*(volatile uint8_t *)(uintptr_t)pa = e[off + j];
		}
		/* [vaddr+filesz, vaddr+memsz) stays zero (bss) — frames were zeroed. */
		if (vend > brk_end) brk_end = vend;     /* track the top for the heap */
		/* (PT_LOAD load trace silenced) */
	}

	*entry_out = rd32(e + E_ENTRY);
	if (brk_out) *brk_out = brk_end;            /* initial program break */
	return 0;
}
