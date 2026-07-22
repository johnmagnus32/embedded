/*
 * uorphaninit.c — PID 1 for the orphan-reparenting / slot-reuse test.
 *
 * Proves BOTH review findings are closed:
 *   (1) orphan reparenting: a child whose parent exits is adopted by init;
 *   (2) no stale-parent-pointer aliasing when the exited parent's PCB SLOT is
 *       reused by a new process.
 *
 * The reviewer's major scenario needs an orphan that is still ALIVE while its
 * parent's freed slot gets reused. In this cooperative kernel a CPU-bound loop
 * never yields, so we hold the orphan alive by BLOCKING it on a pipe that init
 * controls — a real sync primitive.
 *
 * Sequence (deterministic under cooperative scheduling):
 *   init: pipe(p); fork A.
 *   A: fork B; A exits(21)                 -> B reparented to init.
 *   B: close write end; read(p) -> BLOCKS  -> B stays alive (P_BLOCKED), off runq.
 *   init: reap A (code 21)                 -> A's slot freed.
 *   init: fork N (REUSES A's slot); N exits(99); init reaps N (code 99).
 *   init: write(p) -> wakes B; B reads, exits(42); init reaps B (code 42).
 *
 * With the bug (no reparenting): B->parent stays pointing at A's slot; after N
 * reuses it, B is never adopted by init, so B (code 42) is NEVER reaped -> the
 * final wait returns -ECHILD and we print FAIL. With the fix, all three codes
 * are reaped. We assert we saw exactly {21, 99, 42}.
 */

#include <stdint.h>

#define SYS_exit    1
#define SYS_read    3
#define SYS_write   4
#define SYS_close   6
#define SYS_fork    2
#define SYS_pipe   42
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

#define A_EXIT_CODE  21
#define N_EXIT_CODE  99
#define B_EXIT_CODE  42

/* wait4(-1): reap one child, return its exit code (or -1 on -ECHILD). */
static int reap_one(void)
{
	int status = 0;
	long w = usys(SYS_wait4, -1, (long)&status, 0, 0);
	if (w < 0) { puts_("orphaninit: wait4 -> -ECHILD (a child was lost!)\n"); return -1; }
	int code = (status >> 8) & 0xff;
	puts_("orphaninit: reaped pid "); putint((int)w);
	puts_(" code "); putint(code); puts_("\n");
	return code;
}

__attribute__((used))
static void cmain(void)
{
	int p[2];
	if (usys(SYS_pipe, (long)p, 0, 0, 0) < 0) { puts_("orphaninit: pipe FAILED\n"); usys(SYS_exit,1,0,0,0); }

	puts_("orphaninit: pid 1 up; forking child A.\n");
	long a = usys(SYS_fork, 0, 0, 0, 0);
	if (a == 0) {
		/* child A: fork grandchild B, then exit WITHOUT waiting (orphans B). */
		long b = usys(SYS_fork, 0, 0, 0, 0);
		if (b == 0) {
			/* grandchild B: block on the pipe so it stays alive as an orphan
			 * across init's reap-A + fork-N (which reuses A's slot). */
			usys(SYS_close, p[1], 0, 0, 0);       /* B is reader only */
			puts_("orphaninit: [B] blocking on pipe (alive, orphaned).\n");
			char c;
			usys(SYS_read, p[0], (long)&c, 1, 0);  /* BLOCKS until init writes */
			puts_("orphaninit: [B] woken; exiting.\n");
			usys(SYS_exit, B_EXIT_CODE, 0, 0, 0);
		}
		puts_("orphaninit: [A] forked B, exiting WITHOUT waiting.\n");
		usys(SYS_exit, A_EXIT_CODE, 0, 0, 0);
	}

	int seen21 = 0, seen99 = 0, seen42 = 0;

	/* 1) Reap A (B is blocked on the pipe, so the only zombie is A). */
	int c1 = reap_one();
	if (c1 == A_EXIT_CODE) seen21 = 1;

	/* 2) Fork N — it reuses A's just-freed PCB slot. Reap it. If the stale-
	 * pointer bug were present, this slot reuse is what makes B's parent alias N. */
	long n = usys(SYS_fork, 0, 0, 0, 0);
	if (n == 0) usys(SYS_exit, N_EXIT_CODE, 0, 0, 0);
	int c2 = reap_one();
	if (c2 == N_EXIT_CODE) seen99 = 1;

	/* 3) Wake B (write the pipe), then reap it. With the fix B is init's adopted
	 * child; with the bug B is unreapable and reap_one() returns -ECHILD. */
	usys(SYS_write, p[1], (long)"x", 1, 0);
	int c3 = reap_one();
	if (c3 == B_EXIT_CODE) seen42 = 1;

	if (seen21 && seen99 && seen42)
		puts_("orphaninit: REPARENT+SLOTREUSE OK (all of A,N,B reaped correctly)\n");
	else
		puts_("orphaninit: REPARENT FAILED\n");
	usys(SYS_exit, 0, 0, 0, 0);
	for (;;) {}
}

__attribute__((naked, noreturn))
void _ustart(void)
{
	__asm__ volatile("bl cmain\n\t" ::: "lr");
}
