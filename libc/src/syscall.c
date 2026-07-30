/*
 * syscall.c — the raw kernel-entry primitives for gv3libc.
 *
 * ARM Linux EABI: syscall number in r7, args in r0..r6, `svc 0`, return in r0.
 * A negative return in the range [-4095, -1] is a negated errno (Linux
 * convention); the higher-level wrappers turn that into errno + a -1/NULL
 * return. These two functions are the ONLY place the library issues `svc`.
 *
 * Factored from the hand-rolled usys()/ummap2() in the kernel's user programs,
 * which already run on our kernel — this is that proven code, made reusable.
 */
#include "syscall_internal.h"

long __syscall3(long nr, long a0, long a1, long a2)
{
	register long r7 __asm__("r7") = nr;
	register long r0 __asm__("r0") = a0;
	register long r1 __asm__("r1") = a1;
	register long r2 __asm__("r2") = a2;
	__asm__ volatile("svc 0"
	                 : "+r"(r0)
	                 : "r"(r7), "r"(r1), "r"(r2)
	                 : "memory");
	return r0;
}

long __syscall6(long nr, long a0, long a1, long a2, long a3, long a4, long a5)
{
	register long r7 __asm__("r7") = nr;
	register long r0 __asm__("r0") = a0;
	register long r1 __asm__("r1") = a1;
	register long r2 __asm__("r2") = a2;
	register long r3 __asm__("r3") = a3;
	register long r4 __asm__("r4") = a4;
	register long r5 __asm__("r5") = a5;
	__asm__ volatile("svc 0"
	                 : "+r"(r0)
	                 : "r"(r7), "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r5)
	                 : "memory");
	return r0;
}
