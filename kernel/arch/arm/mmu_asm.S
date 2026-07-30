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

/*
 * void mmu_invalidate_dcache_all(void) — invalidate the entire data/unified
 * cache hierarchy by set/way (all levels reported by CLIDR up to LoC).
 *
 * WHY: at kernel entry the previous loader may have left the D-cache ENABLED and
 * populated (mainline U-Boot does; our own bootloader keeps caches off). Merely
 * clearing SCTLR.C turns the cache OFF but leaves those stale lines resident; a
 * later cache-on run — or, more subtly, the table walker / a differently-attributed
 * access — can then hit stale data (this made mmu_selftest read back MISMATCH when
 * booted via U-Boot's bootz). Invalidating by set/way here makes the kernel robust
 * to ANY loader's cache state instead of assuming a clean handoff.
 *
 * INVALIDATE (not clean): we are discarding whatever the loader left — we want the
 * cache empty, and we have no dirty kernel data to preserve this early. This is the
 * canonical ARMv7-A set/way loop (ARM ARM DDI0406 B2, and mainline
 * arch/arm/mm/cache-v7.S v7_invalidate_l1 / __v7_setup). Clobbers r0-r11 (AAPCS:
 * r0-r3,r12 are call-clobbered; r4-r11 are saved/restored).
 */
.global mmu_invalidate_dcache_all
mmu_invalidate_dcache_all:
	push    {r4-r11, lr}
	dmb                                 /* ensure ordering before we start     */
	mrc     p15, 1, r0, c0, c0, 1       /* CLIDR -> r0                          */
	mov     r3, r0, lsr #23             /* r3 = LoC field << 1 ...              */
	ands    r3, r3, #(7 << 1)           /* r3 = LoC * 2 (cache-level index step)*/
	beq     .Lfinished                  /* LoC == 0 -> no caches to invalidate  */
	mov     r10, #0                     /* r10 = current cache level << 1       */
.Lloop_level:
	add     r2, r10, r10, lsr #1        /* r2 = level*3 (CLIDR ctype field pos) */
	mov     r1, r0, lsr r2              /* r1 = CLIDR >> (level*3)               */
	and     r1, r1, #7                  /* r1 = this level's cache type          */
	cmp     r1, #2
	blt     .Lnext_level                /* <2 = no data/unified cache here       */
	mcr     p15, 2, r10, c0, c0, 0      /* CSSELR = select this cache level      */
	isb                                 /* sync the CSSELR change                */
	mrc     p15, 1, r1, c0, c0, 0       /* CCSIDR of the selected cache          */
	and     r2, r1, #7                  /* r2 = log2(line length) - 4            */
	add     r2, r2, #4                  /* r2 = log2(line length in words) + 2   */
	movw    r4, #0x3ff
	ands    r4, r4, r1, lsr #3          /* r4 = max way index (Associativity-1)  */
	clz     r5, r4                      /* r5 = bit position of way field        */
	movw    r7, #0x7fff
	ands    r7, r7, r1, lsr #13         /* r7 = max set index (NumSets-1)        */
.Lloop_way:
	mov     r9, r7                      /* r9 = working set counter              */
.Lloop_set:
	orr     r11, r10, r4, lsl r5        /* r11 = level | (way << way-shift)      */
	orr     r11, r11, r9, lsl r2        /* r11 |= set << set-shift               */
	mcr     p15, 0, r11, c7, c6, 2      /* DCISW: invalidate line by set/way     */
	subs    r9, r9, #1                  /* decrement set                         */
	bge     .Lloop_set
	subs    r4, r4, #1                  /* decrement way                         */
	bge     .Lloop_way
.Lnext_level:
	add     r10, r10, #2                /* next cache level                      */
	cmp     r3, r10
	bgt     .Lloop_level
.Lfinished:
	mov     r10, #0
	mcr     p15, 2, r10, c0, c0, 0      /* CSSELR = 0 (restore selection)        */
	dsb
	isb
	pop     {r4-r11, lr}
	bx      lr

/*
 * void mmu_clean_dcache_all(void) — CLEAN the entire data/unified cache by
 * set/way (all levels to LoC), pushing dirty lines out to memory. Same walk as
 * mmu_invalidate_dcache_all but with DCCSW (clean) instead of DCISW (invalidate),
 * so nothing is discarded. Used by sync_icache: once the D-cache is ENABLED,
 * code the kernel just wrote (execve/fork) sits dirty in the D-cache and has NOT
 * reached the point-of-unification where the I-cache fetches — clean it out
 * first, then invalidate the I-cache. Clobbers r0-r11 (r4-r11 saved/restored).
 */
.global mmu_clean_dcache_all
mmu_clean_dcache_all:
	push    {r4-r11, lr}
	dmb
	mrc     p15, 1, r0, c0, c0, 1       /* CLIDR                                 */
	mov     r3, r0, lsr #23
	ands    r3, r3, #(7 << 1)           /* r3 = LoC * 2                          */
	beq     .Lcln_done
	mov     r10, #0
.Lcln_level:
	add     r2, r10, r10, lsr #1
	mov     r1, r0, lsr r2
	and     r1, r1, #7
	cmp     r1, #2
	blt     .Lcln_next
	mcr     p15, 2, r10, c0, c0, 0      /* CSSELR = level                        */
	isb
	mrc     p15, 1, r1, c0, c0, 0       /* CCSIDR                                */
	and     r2, r1, #7
	add     r2, r2, #4                  /* line length                           */
	movw    r4, #0x3ff
	ands    r4, r4, r1, lsr #3          /* max way                               */
	clz     r5, r4
	movw    r7, #0x7fff
	ands    r7, r7, r1, lsr #13         /* max set                               */
.Lcln_way:
	mov     r9, r7
.Lcln_set:
	orr     r11, r10, r4, lsl r5
	orr     r11, r11, r9, lsl r2
	mcr     p15, 0, r11, c7, c10, 2     /* DCCSW: CLEAN line by set/way          */
	subs    r9, r9, #1
	bge     .Lcln_set
	subs    r4, r4, #1
	bge     .Lcln_way
.Lcln_next:
	add     r10, r10, #2
	cmp     r3, r10
	bgt     .Lcln_level
.Lcln_done:
	mov     r10, #0
	mcr     p15, 2, r10, c0, c0, 0      /* CSSELR = 0                            */
	dsb
	isb
	pop     {r4-r11, lr}
	bx      lr

/*
 * void mmu_flush_caches_tlb(void) — full pre-MMU scrub, called from _start once
 * caches are OFF (SCTLR.M/C/I cleared). Invalidates I-cache, branch predictor,
 * and the TLBs, on top of the D-cache invalidate above. This is the "trust no
 * loader" reset of all address-translation + cache state before we build our own
 * page tables.
 */
.global mmu_flush_caches_tlb
mmu_flush_caches_tlb:
	push    {lr}
	bl      mmu_invalidate_dcache_all   /* D/unified cache invalidate by set/way */
	mov     r0, #0
	mcr     p15, 0, r0, c7, c5, 0       /* ICIALLU: invalidate entire I-cache    */
	mcr     p15, 0, r0, c7, c5, 6       /* BPIALL:  invalidate branch predictor  */
	mcr     p15, 0, r0, c8, c7, 0       /* TLBIALL: invalidate unified TLB       */
	dsb
	isb
	pop     {lr}
	bx      lr

/*
 * void sync_icache(void) — make freshly-WRITTEN code fetchable/executable.
 *
 * The kernel loads a process image by memcpy'ing code bytes into pages (execve /
 * vfs_load_elf) and then jumps to them. Those stores go through the DATA side; the
 * INSTRUCTION side (I-cache, branch predictor, prefetch) can hold stale content
 * for the same addresses — the classic self-modifying-code / freshly-loaded-code
 * hazard. QEMU doesn't model I/D separation so it never bites there, but real
 * Cortex-A7 does (and U-Boot hands off with branch prediction ENABLED, SCTLR.Z=1,
 * so speculation is live). After writing code and BEFORE executing it we must:
 *   DSB  — ensure the code stores have reached memory (drain the write path)
 *   ICIALLU + BPIALL — drop any stale I-cache lines + branch predictions
 *   DSB/ISB — ensure the invalidate completed and refetch
 * This is the ARMv7 equivalent of Linux flush_icache_range() on every exec.
 */
.global sync_icache
sync_icache:
	push    {lr}
	/* Once the D-cache is ENABLED, the code the kernel just wrote (execve/fork
	 * memcpy) is dirty in the D-cache and may NOT have reached the point-of-
	 * unification. Clean the whole D-cache to PoU/PoC first so those bytes are in
	 * memory, THEN invalidate the I-cache + branch predictor so the next fetch
	 * gets the new instructions. (With caches off this clean is a cheap no-op, so
	 * it's safe either way.) */
	bl      mmu_clean_dcache_all        /* dirty code lines -> memory            */
	mov     r0, #0
	mcr     p15, 0, r0, c7, c5, 0       /* ICIALLU: invalidate entire I-cache    */
	mcr     p15, 0, r0, c7, c5, 6       /* BPIALL:  invalidate branch predictor  */
	dsb
	isb
	pop     {lr}
	bx      lr

/*
 * void mmu_enable_caches(void) — turn ON the I-cache and D-cache in SCTLR.
 * Call AFTER the MMU is enabled and AFTER the freshly-built L1 table has been
 * cleaned to memory (the caller does mmu_clean_dcache_all first) so the table
 * walker — which does cacheable walks once caches are on — sees the real table,
 * not stale/absent cache lines. Sets SCTLR.C (bit2) and SCTLR.I (bit12).
 * This is the ~150x speedup: without it every fetch/load/store hits uncached
 * DDR3 (measured: 792KB gunzip took 14.5s cache-off).
 */
.global mmu_enable_caches
mmu_enable_caches:
	dsb
	isb
	mrc     p15, 0, r0, c1, c0, 0       /* SCTLR                                 */
	orr     r0, r0, #(1 << 2)           /* C = 1 (D-cache enable)                */
	orr     r0, r0, #(1 << 12)          /* I = 1 (I-cache enable)                */
	mcr     p15, 0, r0, c1, c0, 0
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
 * void mmu_clean_dcache_line(uint32_t addr) — clean the D-cache line containing
 * `addr` to the point-of-coherency (DCCMVAC), then DSB.
 *
 * REQUIRED for page-table descriptor writes once the D-cache is enabled: TTBR0 is
 * programmed for NON-cacheable table walks, so the hardware walker reads
 * descriptors straight from memory, bypassing the D-cache. A descriptor written
 * with a plain store sits DIRTY in the D-cache and the walker would read the
 * STALE memory value — the new mapping silently wouldn't take effect. Cleaning
 * the descriptor's line to PoC before invalidating the TLB makes the walker see
 * it. (Caches-off: this is a cheap no-op, so callers can always use it.)
 */
.global mmu_clean_dcache_line
mmu_clean_dcache_line:
	mcr     p15, 0, r0, c7, c10, 1    /* DCCMVAC: clean line by MVA to PoC      */
	dsb
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
	/* Invalidate TLB + branch predictor HERE — after TTBR0/TTBCR/DACR are set
	 * (the caller did that) and immediately before enabling the MMU. mmu_init's
	 * earlier TLBIALL ran while TTBR0 still pointed at the PREVIOUS loader's table
	 * (U-Boot's), so between that and here the CPU could speculatively walk the old
	 * table and refill the TLB with stale entries (e.g. a mapping for 0x50000000
	 * that U-Boot set up). Those stale entries then shadow our descriptors —
	 * observed on silicon as mmu_selftest reading a DIFFERENT physical address than
	 * the descriptor maps (via_va sees the write, via_pa reads 0). Invalidating
	 * with the NEW TTBR0 already programmed guarantees the first translated access
	 * walks OUR table. (Our own bootloader has no MMU/TLB state, so this only bit
	 * the U-Boot handoff.) */
	mov     r1, #0
	mcr     p15, 0, r1, c8, c7, 0     /* TLBIALL: drop all (stale) TLB entries  */
	mcr     p15, 0, r1, c7, c5, 6     /* BPIALL:  drop stale branch predictions */
	dsb
	isb
	mrc     p15, 0, r0, c1, c0, 0     /* read SCTLR         */
	orr     r0, r0, #(1 << 0)         /* M = 1 (MMU enable) */
	mcr     p15, 0, r0, c1, c0, 0     /* write SCTLR        */
	isb
	bx      lr
