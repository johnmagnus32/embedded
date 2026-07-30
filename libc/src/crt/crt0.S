/*
 * crt0.S — C runtime startup for gv3libc (ARM, EABI).
 *
 * The kernel's ELF loader (kernel/elf.c) enters a fresh process with the stack
 * pointer aimed at the initial stack frame it built (kernel/proc.c
 * setup_user_stack), laid out low->high exactly like Linux/musl:
 *
 *   [sp+0]  argc
 *   [sp+4]  argv[0], argv[1], ... argv[argc-1]
 *           NULL                       (argv terminator)
 *           envp[0], envp[1], ...
 *           NULL                       (envp terminator)
 *           auxv...                    (we ignore it for now)
 *
 * We capture sp BEFORE touching it, hand argc/argv/envp to __libc_start_main in
 * C, and if main() returns we call _exit(status). This mirrors musl's _start ->
 * __libc_start_main; a real crt0 also zeroes .bss, but our loader hands us
 * zeroed frames for the bss pages already (pmm_alloc_page returns zeroed), so we
 * don't need to — kept as a NOTE, not code, so the assumption is documented.
 */
	.section .text._start, "ax"
	.global _start
	.type _start, %function
_start:
	mov     r0, sp            /* r0 = pointer to the initial stack (argc)      */
	/* Realign the stack to 8 bytes for the AAPCS C call. The kernel already
	 * 8-aligns argc's address, but mask defensively; we don't need to restore
	 * it since __libc_start_main does not return here. */
	bic     sp, sp, #7
	bl      __libc_start_main /* void __libc_start_main(long *initial_sp)      */
	/* __libc_start_main calls _exit; if it ever returns, trap loudly. */
1:	b       1b
	.size _start, . - 1b
