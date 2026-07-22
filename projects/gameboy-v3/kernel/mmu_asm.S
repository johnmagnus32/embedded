/*
 * mmu_asm.S — CP15 MMU register accessors + the enable sequence.
 * Encodings verified vs the ARM ARM (DDI0406) + Cortex-A7 TRM.
 */

.text

/* void mmu_set_ttbr0(uint32_t v) — TTBR0 = table base | walk-attrs (r0). */
.global mmu_set_ttbr0
mmu_set_ttbr0:
	mcr     p15, 0, r0, c2, c0, 0     /* TTBR0 */
	bx      lr

/* void mmu_set_ttbcr(uint32_t v) — TTBCR (0 = N=0, short-descriptor). */
.global mmu_set_ttbcr
mmu_set_ttbcr:
	mcr     p15, 0, r0, c2, c0, 2     /* TTBCR */
	isb
	bx      lr

/* void mmu_set_dacr(uint32_t v) — Domain Access Control Register. */
.global mmu_set_dacr
mmu_set_dacr:
	mcr     p15, 0, r0, c3, c0, 0     /* DACR */
	isb
	bx      lr

/* void mmu_tlbiall(void) — invalidate the entire unified TLB. */
.global mmu_tlbiall
mmu_tlbiall:
	mov     r0, #0
	mcr     p15, 0, r0, c8, c7, 0     /* TLBIALL */
	dsb
	isb
	bx      lr

/* void mmu_tlbimva(uint32_t va) — invalidate one VA (unified TLB, by MVA). */
.global mmu_tlbimva
mmu_tlbimva:
	mcr     p15, 0, r0, c8, c7, 1     /* TLBIMVA */
	dsb
	isb
	bx      lr

/*
 * void mmu_enable_sctlr(void) — turn the MMU on.
 * Assumes TTBR0/TTBCR/DACR are already set and the L1 table is built. We drain
 * writes (DSB) and synchronize (ISB) before flipping SCTLR.M so the table is
 * visible to the table walker; ISB after so the next fetch is translated.
 * We deliberately do NOT enable the D-cache (C) or I-cache (I) yet — running
 * cacheable would require cleaning the freshly-built table out of the D-cache
 * first; keeping caches off is simplest and correct for S6.
 */
.global mmu_enable_sctlr
mmu_enable_sctlr:
	dsb
	isb
	mrc     p15, 0, r0, c1, c0, 0     /* read SCTLR         */
	orr     r0, r0, #(1 << 0)         /* M = 1 (MMU enable) */
	mcr     p15, 0, r0, c1, c0, 0     /* write SCTLR        */
	isb
	bx      lr
