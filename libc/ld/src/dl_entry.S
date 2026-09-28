/*
 * dl_entry.S — the entry point of ld.so.1 (the dynamic linker).
 *
 * The kernel loads us as the PT_INTERP of a dynamic binary and jumps to our
 * e_entry with the same initial stack as a normal program:
 *   [sp+0] argc, argv[], NULL, envp[], NULL, auxv[]
 *
 * We capture sp (before any prologue clobbers it) and pass it to _dl_main in C.
 *
 * SELF-RELOCATION: built by our -fPIC toolchain, ld.so's own string/global refs go through the GOT, so
 * it carries R_ARM_RELATIVE entries whose GOT slots must be biased BEFORE any C runs (else the first
 * `dl_puts` reads a wrong string pointer). So `_start` is its own micro-loader: take the load bias,
 * walk our .dynamic for DT_REL/DT_RELSZ, add the bias to each RELATIVE slot, THEN hand off to _dl_main.
 * (Same algorithm as the toolchain's -pie crt.)
 */
	.section .text._start, "ax"
	.global _start
	.type   _start, %function
_start:
	/* --- self-relocate: apply our own R_ARM_RELATIVE before any C code touches the GOT ---
	 * Bias comes from the auxv's AT_BASE (the kernel tells us where it loaded us); _DYNAMIC is found
	 * PC-relatively. No `.text` word needs a fixup, so this bootstrap writes ONLY the .got (RW): ld.so.1
	 * has ZERO text relocations and works under a W^X kernel that maps .text read-only. */
	mov     r4, sp            /* r4 = &argc */
	ldr     r5, [r4]          /* argc */
	add     r4, r4, #4        /* &argv[0] */
	add     r4, r4, r5, lsl #2 /* skip argv[0..argc-1] -> &argv[argc] (the NULL) */
	add     r4, r4, #4        /* -> &envp[0] */
.Lenv:
	ldr     r0, [r4]
	add     r4, r4, #4
	cmp     r0, #0
	bne     .Lenv             /* skip envp to its NULL -> r4 = &auxv[0] */
	mov     r6, #0            /* r6 = bias (fallback 0) */
.Laux:
	ldr     r0, [r4]          /* a_type */
	ldr     r1, [r4, #4]      /* a_val  */
	add     r4, r4, #8
	cmp     r0, #0            /* AT_NULL */
	beq     .Lauxdone
	cmp     r0, #7            /* AT_BASE = our load bias */
	moveq   r6, r1
	b       .Laux
.Lauxdone:
	ldr     r7, .Ldynamic
	add     r7, pc, r7        /* r7 = runtime &_DYNAMIC (pc reads as the .word below) */
	b       .Ldynamic_end
.Ldynamic: .word _DYNAMIC - .   /* R_ARM_REL32: an in-image distance, fixed at link time (no runtime fixup) */
.Ldynamic_end:
	mov     r8, #0            /* r8 = rel-table link-time vaddr */
	mov     r9, #0            /* r9 = rel-table size (bytes)   */
.Lscan:
	ldr     r0, [r7]          /* d_tag */
	ldr     r1, [r7, #4]      /* d_val */
	add     r7, r7, #8
	cmp     r0, #0            /* DT_NULL */
	beq     .Lscandone
	cmp     r0, #17           /* DT_REL   */
	moveq   r8, r1
	cmp     r0, #18           /* DT_RELSZ */
	moveq   r9, r1
	b       .Lscan
.Lscandone:
	add     r8, r8, r6        /* runtime &rel[0]        */
	add     r9, r8, r9        /* runtime end-of-table   */
.Lrel:
	cmp     r8, r9
	bcs     .Lreldone         /* r8 >= r9 -> done */
	ldr     r0, [r8]          /* r_offset */
	ldr     r1, [r8, #4]      /* r_info   */
	add     r8, r8, #8
	and     r1, r1, #0xff     /* reloc type */
	cmp     r1, #23           /* R_ARM_RELATIVE */
	bne     .Lrel
	add     r2, r0, r6        /* runtime target word    */
	ldr     r3, [r2]
	add     r3, r3, r6        /* += bias                */
	str     r3, [r2]
	b       .Lrel
.Lreldone:
	/* --- image self-consistent: hand the ORIGINAL stack to _dl_main --- */
	mov     r0, sp        /* r0 = pointer to argc (self-reloc used only r4-r9, so sp is intact) */
	bic     sp, sp, #7    /* 8-align for the C call (AAPCS) */
	bl      _dl_main      /* _dl_main(long *sp) — does not return */
1:	b       1b            /* if it ever returns: trap */
	.size _start, .-_start


	/* Mark the stack non-executable (silences ld's missing-.note.GNU-stack warning). */
	.section .note.GNU-stack,"",%progbits
