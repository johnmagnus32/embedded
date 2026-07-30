/*
 * dl_entry.S — the entry point of ld-gv3.so.1 (the dynamic linker).
 *
 * The kernel loads us as the PT_INTERP of a dynamic binary and jumps to our
 * e_entry with the same initial stack as a normal program:
 *   [sp+0] argc, argv[], NULL, envp[], NULL, auxv[]
 *
 * We capture sp (before any prologue clobbers it) and pass it to _dl_main in C.
 * No self-relocation is needed: our binaries have zero R_ARM_RELATIVE (verified).
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
	mov     r0, sp        /* r0 = pointer to argc (the initial stack) */
	bic     sp, sp, #7    /* 8-align for the C call (AAPCS) */
	bl      _dl_main      /* _dl_main(long *sp) — does not return */
1:	b       1b            /* if it ever returns: trap */
	.size _start, .-_start

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
