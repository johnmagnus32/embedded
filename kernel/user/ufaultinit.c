/*
 * ufaultinit.c — PID 1 for the fault-isolation test.
 *
 * Forks TWO children in turn, each of which deliberately faults (execs
 * /bin/fault, which writes through NULL). The parent wait4()s each and prints
 * the status. If the kernel's real fault handling works, each child is killed by
 * SIGSEGV (status low 7 bits = 11) and the PARENT keeps running.
 *
 * The SECOND fault is the important one: it proves the abort-mode banked SP is
 * RESET on every fault entry. An earlier bug reset it only on the first fault
 * (user_return left the banked SP pointing into a PCB), so the second data abort
 * would build its trapframe on top of a live PCB and corrupt it. Two faults in
 * a row exercising a clean handler each time is the regression test for that.
 *
 * The golden test greps for both children's pre-fault lines, both kernel
 * "killed by data-abort" lines, both parent survival lines, and a clean halt.
 */

#include <stdint.h>

#define SYS_exit    1
#define SYS_write   4
#define SYS_fork    2
#define SYS_execve 11
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
static void putint(int v){char b[12];int i=0;unsigned u=v<0?-v:v;if(v<0)puts_("-");
	do{b[i++]=(char)('0'+u%10);u/=10;}while(u);while(i){i--;usys(SYS_write,1,(long)&b[i],1,0);}}

/* Fork one child that execs /bin/fault (faults via NULL write) and wait for it;
 * print the killed-by-signal result. Returns the reaped signal number. */
static int fork_faulting_child(int round)
{
	long pid = usys(SYS_fork, 0, 0, 0, 0);
	if (pid == 0) {
		char *av[] = { "/bin/fault", 0 };
		char *ev[] = { 0 };
		usys(SYS_execve, (long)"/bin/fault", (long)av, (long)ev, 0);
		puts_("faultinit: [child] execve FAILED\n");
		usys(SYS_exit, 127, 0, 0, 0);
	}
	int status = 0;
	long w = usys(SYS_wait4, pid, (long)&status, 0, 0);
	int sig = status & 0x7f;                 /* low 7 bits = terminating signal */
	puts_("faultinit: PARENT SURVIVED round ");
	putint(round);
	puts_(". child pid ");
	putint((int)w);
	puts_(" killed by signal ");
	putint(sig);
	puts_("\n");
	return sig;
}

__attribute__((used))
static void cmain(void)
{
	puts_("faultinit: I am pid 1; forking children that will fault.\n");
	/* Two faults in a row: the second proves the banked abort SP is reset per
	 * fault (not just on the first one). */
	fork_faulting_child(1);
	fork_faulting_child(2);
	puts_("faultinit: fault-isolation test complete, exiting 0\n");
	usys(SYS_exit, 0, 0, 0, 0);
	for (;;) {}
}

__attribute__((naked, noreturn))
void _ustart(void)
{
	__asm__ volatile("bl cmain\n\t" ::: "lr");
}
