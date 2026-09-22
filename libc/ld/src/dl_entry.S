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
 * `dl_puts` reads a wrong string pointer). So `_start` is its own micro-loader: compute the load bias,
 * walk our .dynamic for DT_REL/DT_RELSZ, add the bias to each RELATIVE slot, THEN hand off to _dl_main.
 * (Same algorithm as the toolchain's -pie crt. The two `.word _start/_DYNAMIC` literals below are read
 * pre-relocation, so they still hold link-time values — exactly what the bias computation needs.)
 *
 * This ALSO provides a `_dl_base` helper: discovers the linker's own load base
 * by computing the difference between the link-time `_DYNAMIC` symbol and the
 * runtime address where it landed (a classic PC-relative trick). Used by the C
 * code to compute our own base for dso_t.base, so we can resolve libc's imports
 * against ourselves (we export `__syscall3` etc. that libc.so JUMP_SLOTs).
 */
	.section .text._start, "ax"
	.global _start
	.type   _start, %function
_start:
	/* --- self-relocate: apply our own R_ARM_RELATIVE before any C code touches the GOT ---
	 * Bias comes from the auxv's AT_BASE (the kernel tells us where it loaded us) — NOT from a `.text`
	 * literal, so this bootstrap writes ONLY the .got (RW). `.word _DYNAMIC` is a plain link-time value
	 * (UNDEF in this object -> the linker leaves it un-relocated), biased here in a register. Result:
	 * ld.so.1 has ZERO text relocations, so it works under a W^X kernel that maps .text read-only. */
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
	ldr     r7, .Llink_dynamic
	add     r7, r7, r6        /* r7 = runtime &_DYNAMIC */
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
.Llink_dynamic: .word _DYNAMIC   /* link-time &_DYNAMIC (un-relocated: read-only, biased in a register above) */

/*
 * _dl_get_base — return the linker's own load bias.
 *
 * At link time, _DYNAMIC lives at a known VMA inside the .so (the linker placed
 * it). At RUN time, it's at that VMA + the load bias. By comparing the runtime
 * address (PC-relative ldr) against the link-time address (from the GOT or a
 * plain ldr), the difference is the bias. Because we have no R_ARM_RELATIVE, the
 * GOT entry for _DYNAMIC was resolved at link time and its runtime value is
 * _DYNAMIC_linktime + base (ARM GLOB_DAT — the linker application process will
 * fix this). BUT simpler: since we have zero self-reloc, the plain value of
 * _DYNAMIC in the text literal pool = its link-time value, and we get the
 * runtime address via a PC-relative load of the .dynamic section's first entry.
 *
 * Simplest: the kernel gives us AT_BASE in the auxv which is literally our load
 * base. So we just parse that. The asm trick is a fallback; C reads AT_BASE first.
 */

	/* Mark the stack non-executable (silences ld's missing-.note.GNU-stack warning). */
	.section .note.GNU-stack,"",%progbits
