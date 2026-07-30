/*
 * vm.c — per-process address spaces with 4 KB paging (ARMv7 short-descriptor).
 *
 * Each address space is a 16 KB L1 table. We start it as a copy of the kernel's
 * base identity map (so kernel code/stack/MMIO stay reachable after a TTBR0
 * switch), then install L2 (coarse) page tables for user VAs to map individual
 * 4 KB pages.
 *
 * Descriptor formats (verified vs ARM ARM short-descriptor, distinct from the
 * 1 MB SECTION format in mmu.c):
 *   L1 coarse (points to an L2 table): (l2_pa & 0xFFFFFC00) | 0x01   (domain 0)
 *   L2 small page (4 KB), normal WB-WA, USER+kernel RW, executable:
 *       (pa & 0xFFFFF000) | 0x7E
 *       [XN=0, type bit1=1, B=1, C=1, AP[1:0]=0b11, TEX=0b001, AP2=0]
 *   AP[2:0]=0b011 = PL0 (user) AND PL1 (kernel) read/write — user-accessible,
 *   unlike the kernel-only sections.
 *
 * pmm<->pagetable consistency: every user page and every L2 table is tracked by
 * pmm (allocated) and referenced by the tables (mapped). vm_destroy walks the
 * tables and frees exactly what it mapped — the one teardown path.
 */

#include <stdint.h>
#include "vm.h"
#include "pmm.h"
#include "mmu.h"

int printf(const char *fmt, ...);
extern void mmu_set_ttbr0(uint32_t v);
extern void mmu_tlbiall(void);
extern void mmu_tlbimva(uint32_t va);
extern void mmu_clean_dcache_line(uint32_t addr);   /* clean one line to PoC */

/* Clean a range of freshly-written page-table descriptors to memory. TTBR0 walks
 * are NON-cacheable, so the hardware walker reads descriptors from memory; with
 * the D-cache enabled a plain store leaves the descriptor dirty in the cache and
 * the walker would read the stale value. Clean every cache line the [base,base+n)
 * table occupies. (Caches off -> cheap no-op.) Lines are 64 B on Cortex-A7; step
 * by 32 to be safe on any line size. */
static void clean_table(uint32_t base, uint32_t bytes)
{
	for (uint32_t off = 0; off < bytes; off += 32)
		mmu_clean_dcache_line(base + off);
}

#define L1_ENTRIES   4096u
#define L1_SIZE      16384u
#define L1_ALIGN     16384u
#define L2_ENTRIES   256u
#define L2_SIZE      1024u
#define L2_ALIGN     1024u

#define L1_TYPE_MASK   0x3u
#define L1_TYPE_COARSE 0x1u
#define L1_TYPE_SECT   0x2u
#define L1_COARSE(l2)  (((l2) & 0xFFFFFC00u) | 0x01u)   /* domain 0 */
#define L1_COARSE_L2BASE(d)  ((d) & 0xFFFFFC00u)

#define L2_SMALL_USER  0x7Eu                             /* normal, user RW, exec */
#define L2_SMALL_BASE(d) ((d) & 0xFFFFF000u)
#define L2_IS_VALID(d)   (((d) & 0x3u) != 0)

static inline volatile uint32_t *ptr(uint32_t pa) { return (volatile uint32_t *)(uintptr_t)pa; }

/* index helpers */
static inline uint32_t l1_index(uint32_t va) { return va >> 20; }          /* VA[31:20] */
static inline uint32_t l2_index(uint32_t va) { return (va >> 12) & 0xFF; } /* VA[19:12] */

uint32_t vm_create(void)
{
	uint32_t l1 = pmm_alloc_aligned(L1_SIZE / PAGE_SIZE, L1_ALIGN);
	if (!l1)
		return 0;
	/* Copy the kernel's base identity map so the kernel remains mapped in
	 * this address space (kernel code/stack/MMIO all keep working). */
	volatile uint32_t *dst = ptr(l1);
	volatile uint32_t *ker = ptr(mmu_l1_base());
	for (uint32_t i = 0; i < L1_ENTRIES; i++)
		dst[i] = ker[i];
	clean_table(l1, L1_SIZE);          /* make the new L1 visible to the walker */
	return l1;
}

/* Get (or create) the L2 table for `va` in `l1_pa`. Returns L2 physical base. */
static uint32_t l2_for(uint32_t l1_pa, uint32_t va, int create)
{
	volatile uint32_t *l1 = ptr(l1_pa);
	uint32_t i = l1_index(va);
	uint32_t d = l1[i];

	if ((d & L1_TYPE_MASK) == L1_TYPE_COARSE)
		return L1_COARSE_L2BASE(d);

	if (!create)
		return 0;

	/* Slot is currently a section (kernel identity) or invalid — replace with
	 * a fresh, zeroed L2 table. (For user VAs the identity section is device
	 * memory we don't want anyway; we're taking this 1 MB for user paging.) */
	uint32_t l2 = pmm_alloc_aligned(1, L2_ALIGN);   /* 1KB fits in one 4KB frame */
	if (!l2)
		return 0;
	volatile uint32_t *t = ptr(l2);
	for (uint32_t k = 0; k < L2_ENTRIES; k++)
		t[k] = 0;
	clean_table(l2, L2_SIZE);              /* zeroed L2 -> memory                */
	l1[i] = L1_COARSE(l2);
	mmu_clean_dcache_line((uint32_t)(uintptr_t)&l1[i]);   /* new coarse entry    */
	/* BREAK-BEFORE-MAKE: this slot was a VALID 1 MB section (copied from the
	 * kernel identity map as a Device section for VA < DRAM_BASE). We just changed
	 * its block size (1 MB section -> 4 KB pages via a coarse entry). If this AS is
	 * live, a speculatively-cached 1 MB section TLB entry would shadow the new page
	 * mappings and send heap/mmap accesses to the wrong (Device-attributed) PA —
	 * silent corruption on real silicon (invisible on QEMU). Invalidate the whole
	 * 1 MB VA so the next access re-walks to the coarse table. */
	mmu_tlbimva(va & 0xFFF00000u);
	return l2;
}

int vm_map_page(uint32_t l1_pa, uint32_t va, uint32_t pa)
{
	if (va < USER_VA_MIN || va >= USER_VA_MAX)
		return -1;
	uint32_t l2 = l2_for(l1_pa, va, 1);
	if (!l2)
		return -2;
	volatile uint32_t *t = ptr(l2);
	t[l2_index(va)] = L2_SMALL_BASE(pa) | L2_SMALL_USER;
	mmu_clean_dcache_line((uint32_t)(uintptr_t)&t[l2_index(va)]);  /* PTE -> memory */
	return 0;
}

uint32_t vm_walk(uint32_t l1_pa, uint32_t va)
{
	uint32_t l2 = l2_for(l1_pa, va, 0);
	if (!l2)
		return 0;
	uint32_t d = ptr(l2)[l2_index(va)];
	if (!L2_IS_VALID(d))
		return 0;
	return L2_SMALL_BASE(d) | (va & 0xFFFu);
}

/* Clear the L2 entry for `va` (does NOT free the physical frame — the caller
 * owns that). Flushes the TLB for this VA so the unmap takes effect. Used by
 * munmap/brk-shrink. The L2 table itself is left in place (reused later). */
int vm_unmap_page(uint32_t l1_pa, uint32_t va)
{
	uint32_t l2 = l2_for(l1_pa, va, 0);
	if (!l2)
		return -1;
	volatile uint32_t *t = ptr(l2);
	if (!L2_IS_VALID(t[l2_index(va)]))
		return -1;
	t[l2_index(va)] = 0;
	/* Clean the cleared descriptor to memory (non-cacheable walks read from
	 * memory, so a dirty line would leave the OLD mapping live), then the ARM ARM
	 * B3.10.1 sequence: store; DSB; TLBI; DSB; ISB (the trailing DSB+ISB live in
	 * mmu_tlbimva; mmu_clean_dcache_line ends in a DSB, covering the leading one). */
	mmu_clean_dcache_line((uint32_t)(uintptr_t)&t[l2_index(va)]);
	mmu_tlbimva(va & ~0xFFFu);
	return 0;
}

void vm_destroy(uint32_t l1_pa)
{
	volatile uint32_t *l1 = ptr(l1_pa);
	/* Only the USER VA range owns pages; kernel slots are shared identity. */
	for (uint32_t va = USER_VA_MIN & ~0xFFFFFu; va < USER_VA_MAX; va += 0x100000u) {
		uint32_t i = l1_index(va);
		uint32_t d = l1[i];
		if ((d & L1_TYPE_MASK) != L1_TYPE_COARSE)
			continue;                       /* not a user-paged 1MB */
		uint32_t l2 = L1_COARSE_L2BASE(d);
		volatile uint32_t *t = ptr(l2);
		for (uint32_t k = 0; k < L2_ENTRIES; k++) {
			if (L2_IS_VALID(t[k]))
				pmm_free_page(L2_SMALL_BASE(t[k]));   /* free the user page */
		}
		pmm_free_page(l2);                  /* free the L2 table itself */
		l1[i] = 0;
	}
	/* The L1 table is 16 KB = 4 physical frames; free all of them. */
	for (uint32_t f = 0; f < L1_SIZE / PAGE_SIZE; f++)
		pmm_free_page(l1_pa + f * PAGE_SIZE);
}

int vm_copy(uint32_t dst_l1, uint32_t src_l1)
{
	volatile uint32_t *s = ptr(src_l1);
	for (uint32_t va = USER_VA_MIN & ~0xFFFFFu; va < USER_VA_MAX; va += 0x100000u) {
		uint32_t d = s[l1_index(va)];
		if ((d & L1_TYPE_MASK) != L1_TYPE_COARSE)
			continue;
		volatile uint32_t *st = ptr(L1_COARSE_L2BASE(d));
		for (uint32_t k = 0; k < L2_ENTRIES; k++) {
			if (!L2_IS_VALID(st[k]))
				continue;
			uint32_t src_pa = L2_SMALL_BASE(st[k]);
			uint32_t page_va = (va & 0xFFF00000u) | (k << 12);
			uint32_t np = pmm_alloc_page();
			if (!np)
				return -1;
			/* copy the 4 KB page contents (identity map lets us touch PAs) */
			volatile uint32_t *src = ptr(src_pa);
			volatile uint32_t *nw  = ptr(np);
			for (uint32_t w = 0; w < PAGE_SIZE / 4; w++)
				nw[w] = src[w];
			if (vm_map_page(dst_l1, page_va, np) != 0)
				return -2;
		}
	}
	return 0;
}

void vm_switch(uint32_t l1_pa)
{
	mmu_set_ttbr0(l1_pa);
	mmu_tlbiall();
}
