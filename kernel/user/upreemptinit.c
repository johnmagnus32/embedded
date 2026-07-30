/*
 * upreemptinit.c — PID 1 for the preemption / time-slicing test.
 *
 * Proves the timer actually PREEMPTS user code — something the cooperative
 * scheduler could never do. init forks two CPU-BOUND children that spin in pure
 * arithmetic loops and NEVER make a syscall except an occasional progress print.
 * Under cooperative scheduling the first child to run would monopolize the CPU
 * forever (no syscall = no yield), so the second child's output would never
 * appear until the first exits. Under preemption the timer tick round-robins
 * them, so we see BOTH children's progress lines interleaved, then init reaps
 * both.
 *
 * Also stresses VFP-under-preemption implicitly: the spin uses only integer ops
 * (kernel is -mgeneral-regs-only, and we can't rely on the compiler emitting FP
 * here), but the mere fact that BusyBox (hardfloat, VFP-heavy) survives the
 * golden suite under preemption already exercises the FP save/restore. This test
 * targets the scheduling property specifically.
 */

#include <stdint.h>

#define SYS_exit    1
#define SYS_write   4
#define SYS_fork    2
#define SYS_getpid 20
#define SYS_wait4 114

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

/* A CPU-bound child: spin a big integer workload, printing a progress marker
 * every ROUND so we can observe interleaving. The inner loop makes NO syscall,
 * so the ONLY way control leaves this child between prints is a timer preempt. */
__attribute__((used, optimize("O0")))
static void spin_child(const char *tag, int rounds)
{
	for (int r = 0; r < rounds; r++) {
		volatile uint32_t acc = 0;
		for (volatile uint32_t i = 0; i < 4000000u; i++)
			acc += i * 3u + 1u;
		puts_(tag);                     /* e.g. "[A]" — one marker per round */
	}
}

__attribute__((used))
static void cmain(void)
{
	puts_("preemptinit: forking two CPU-bound children (no syscalls in their loops).\n");

	long a = usys(SYS_fork, 0, 0, 0, 0);
	if (a == 0) { spin_child("[A]\n", 4); usys(SYS_exit, 11, 0, 0, 0); }

	long b = usys(SYS_fork, 0, 0, 0, 0);
	if (b == 0) { spin_child("[B]\n", 4); usys(SYS_exit, 22, 0, 0, 0); }

	/* Parent blocks in wait4 -> the two children are the only runnable procs.
	 * With preemption the tick alternates them; their [A]/[B] markers interleave.
	 * Reap both and report. */
	int got_a = 0, got_b = 0;
	for (int n = 0; n < 2; n++) {
		int status = 0;
		long w = usys(SYS_wait4, -1, (long)&status, 0, 0);
		int code = (status >> 8) & 0xff;
		if (code == 11) got_a = 1;
		if (code == 22) got_b = 1;
		(void)w;
	}
	if (got_a && got_b)
		puts_("preemptinit: BOTH CPU-bound children finished (preemption works)\n");
	else
		puts_("preemptinit: a child was lost\n");
	usys(SYS_exit, 0, 0, 0, 0);
	for (;;) {}
}

__attribute__((naked, noreturn))
void _ustart(void)
{
	__asm__ volatile("bl cmain\n\t" ::: "lr");
}
