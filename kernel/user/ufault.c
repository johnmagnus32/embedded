/*
 * ufault.c — a program that deliberately faults, to prove fault isolation.
 *
 * Writes through a NULL pointer (VA 0 is intentionally unmapped — see
 * USER_VA_MIN in vm.h), which raises a Data Abort. Without real fault handling
 * the whole kernel would spin; with it, THIS process is killed (SIGSEGV) and the
 * parent's wait4 reports the killed-by-signal status while the system keeps
 * running. Self-contained, no libc.
 */

#include <stdint.h>

#define SYS_exit    1
#define SYS_write   4

static inline long usys(long nr, long a0, long a1, long a2, long a3)
{
	register long r7 __asm__("r7") = nr;
	register long r0 __asm__("r0") = a0;
	register long r1 __asm__("r1") = a1;
	register long r2 __asm__("r2") = a2;
	register long r3 __asm__("r3") = a3;
	__asm__ volatile("svc 0" : "+r"(r0) : "r"(r7),"r"(r1),"r"(r2),"r"(r3) : "memory");
	return r0;
}
static unsigned slen(const char *s){unsigned n=0;while(s[n])n++;return n;}
static void puts_(const char *s){usys(SYS_write,1,(long)s,slen(s),0);}

__attribute__((used))
static void cmain(void)
{
	puts_("fault: about to write through a NULL pointer...\n");
	volatile unsigned *p = (volatile unsigned *)0;   /* VA 0 = unmapped */
	*p = 0xdeadbeef;                                 /* -> Data Abort   */
	/* Should never reach here — the kernel kills us on the abort. */
	puts_("fault: SURVIVED the null write (BUG: fault not taken)\n");
	usys(SYS_exit, 0, 0, 0, 0);
	for (;;) {}
}

__attribute__((naked, noreturn))
void _ustart(void)
{
	__asm__ volatile("bl cmain\n\t" ::: "lr");
}
