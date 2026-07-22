/*
 * mmu.c — ARMv7-A short-descriptor MMU: identity map + enable.
 *
 * We use the simplest correct scheme: a single 16 KB L1 table (4096 x 1 MB
 * section descriptors, TTBR0-only, TTBCR.N=0 so TTBR0 covers all 4 GB). Every
 * 1 MB of the address space gets one section descriptor. We identity-map
 * everything (VA == PA) so the kernel keeps running the instant the MMU turns
 * on, with region-appropriate memory attributes:
 *   - DRAM (0x40000000..)      -> Normal, write-back cacheable, RW
 *   - everything else (MMIO)   -> Device, uncached, execute-never
 *
 * Descriptor constants verified vs the ARM ARM (short-descriptor format):
 *   NORMAL section = (pa & 0xFFF00000) | 0x1C0E
 *       [1]=section, B=1, C=1, AP[1:0]=0b11 (full RW), TEX=0b001, XN=0, Dom0]
 *   DEVICE section = (pa & 0xFFF00000) | 0x0C16
 *       [1]=section, B=1, C=0, AP[1:0]=0b11, TEX=0b000, XN=1, Dom0]
 * Enable regs: TTBR0 (c2,c0,0), TTBCR=0 (c2,c0,2), DACR=0x55555555 (c3,c0,0),
 * SCTLR.M=bit0. Sequence: table -> TLBIALL -> DACR -> TTBCR -> TTBR0 -> DSB/ISB
 * -> set SCTLR.M -> ISB.
 */

#include <stdint.h>
#include "mmu.h"
#include "pmm.h"

int printf(const char *fmt, ...);

#define L1_ENTRIES   4096u
#define L1_SIZE      (L1_ENTRIES * 4u)   /* 16 KB */
#define L1_ALIGN     16384u

#define SECT_NORMAL_ATTR   0x1C0Eu
#define SECT_DEVICE_ATTR   0x0C16u
#define SECT_MASK          0xFFF00000u

/* Regions we identity-map as normal cacheable RAM (everything else = device). */
#define DRAM_BASE   0x40000000u
#define DRAM_END    0x48000000u          /* 128 MB */

static volatile uint32_t *l1;            /* pointer to the live L1 table */
static uint32_t l1_pa;

/* --- CP15 / barrier helpers (in mmu_asm.S) --- */
extern void mmu_set_ttbr0(uint32_t v);
extern void mmu_set_ttbcr(uint32_t v);
extern void mmu_set_dacr(uint32_t v);
extern void mmu_tlbiall(void);
extern void mmu_enable_sctlr(void);      /* set SCTLR.M with barriers */
extern void mmu_tlbimva(uint32_t va);    /* invalidate one VA in the TLB */

static uint32_t sect_attr(enum mem_type t)
{
	return (t == MEM_NORMAL) ? SECT_NORMAL_ATTR : SECT_DEVICE_ATTR;
}

void mmu_map_section(uint32_t va, uint32_t pa, enum mem_type type)
{
	uint32_t idx = va >> 20;                     /* VA[31:20] indexes L1 */
	l1[idx] = (pa & SECT_MASK) | sect_attr(type);
	mmu_tlbimva(va & SECT_MASK);                 /* drop stale TLB entry */
}

void mmu_init(void)
{
	/* Allocate the 16 KB, 16 KB-aligned L1 table from physical memory. */
	l1_pa = pmm_alloc_aligned(L1_SIZE / PAGE_SIZE, L1_ALIGN);
	if (!l1_pa) {
		printf("mmu: FAILED to allocate L1 table\n");
		return;
	}
	l1 = (volatile uint32_t *)(uintptr_t)l1_pa;

	/* Fill every 1 MB section of the 4 GB space, identity-mapped. */
	for (uint32_t i = 0; i < L1_ENTRIES; i++) {
		uint32_t pa = i << 20;
		enum mem_type t = (pa >= DRAM_BASE && pa < DRAM_END)
		                    ? MEM_NORMAL : MEM_DEVICE;
		l1[i] = (pa & SECT_MASK) | sect_attr(t);
	}
	printf("mmu: L1 table @ 0x%x (identity map, DRAM normal / MMIO device)\n", l1_pa);

	/* Program the MMU registers and enable. Order matters. */
	mmu_tlbiall();                 /* clear any stale TLB entries         */
	mmu_set_dacr(0x55555555u);     /* all domains = client (AP checked)   */
	mmu_set_ttbcr(0);              /* N=0: TTBR0 covers all VAs           */
	mmu_set_ttbr0(l1_pa);          /* table base, non-cacheable walks     */
	mmu_enable_sctlr();            /* DSB/ISB, set SCTLR.M, ISB           */

	printf("mmu: enabled (translation on)\n");
}

uint32_t mmu_l1_base(void) { return l1_pa; }
