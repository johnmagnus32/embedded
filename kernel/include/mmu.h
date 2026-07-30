/*
 * mmu.h — ARMv7-A short-descriptor MMU (1 MB sections, TTBR0-only).
 */
#ifndef GV3K_MMU_H
#define GV3K_MMU_H

#include <stdint.h>

/* Memory type for a 1 MB section mapping. */
enum mem_type {
	MEM_NORMAL,   /* cacheable write-back RAM        */
	MEM_DEVICE,   /* MMIO — uncached, execute-never  */
};

/* Build the identity page table (all 4 GB, region-appropriate attrs) and
 * enable the MMU. After this returns, translation is on (VA == PA). */
void mmu_init(void);

/* Map one 1 MB section: va (1MB-aligned) -> pa (1MB-aligned), with `type`.
 * Updates the live L1 table + flushes the TLB for that VA. */
void mmu_map_section(uint32_t va, uint32_t pa, enum mem_type type);

/* Physical base of the active L1 table (for inspection/tests). */
uint32_t mmu_l1_base(void);

#endif /* GV3K_MMU_H */
