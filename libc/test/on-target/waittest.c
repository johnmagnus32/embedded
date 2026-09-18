/*
 * waittest.c — regression test for the waitpid()/WNOHANG semantics bug.
 *
 * Runs as PID 1 on the mainline reference kernel (which honors WNOHANG). We fork a child
 * that NEVER exits, then call waitpid(-1, &st, WNOHANG). With WNOHANG correctly forwarded
 * to wait4(2), this returns 0 immediately (a child exists but hasn't changed state) and we
 * print WNOHANG_OK. If waitpid dropped its options (the bug), the call becomes a BLOCKING
 * wait for a child that never changes state -> it hangs forever -> no marker -> the harness
 * fails on timeout. So the marker's presence is a clean pass/fail discriminator.
 */
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>

int main(void)
{
	pid_t c = fork();
	if (c < 0) { puts("WNOHANG_FAIL: fork"); _exit(1); }
	if (c == 0) {
		for (;;) __asm__ __volatile__("");   /* child: spin forever, never exit (never a zombie) */
	}

	int st = 0;
	pid_t r = waitpid(-1, &st, WNOHANG);     /* must NOT block: child is alive, no state change */
	if (r == 0) puts("WNOHANG_OK");
	else        printf("WNOHANG_FAIL: waitpid returned %d\n", (int)r);

	_exit(0);   /* PID 1 exits -> kernel panics (panic=1) -> QEMU terminates */
}
