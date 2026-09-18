/*
 * signal.c — sigset ops (pure C) + sigprocmask/signal/kill over the rt_* syscalls.
 */
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>   /* getpid (raise) */
#include "syscall_internal.h"

int sigemptyset(sigset_t *s) { s->__bits[0] = s->__bits[1] = 0; return 0; }
int sigfillset(sigset_t *s)  { s->__bits[0] = s->__bits[1] = ~0UL; return 0; }

/* Locate sig's word+bit in the 64-bit set. Returns bit index (0..31) or -1 if out of range. */
static int sig_bit(int sig, unsigned *word)
{
	if (sig < 1 || sig > 64) return -1;
	*word = (unsigned)(sig - 1) / 32;
	return (sig - 1) % 32;
}

int sigaddset(sigset_t *s, int sig)
{
	unsigned w; int b = sig_bit(sig, &w);
	if (b < 0) { errno = EINVAL; return -1; }
	s->__bits[w] |= 1UL << b;
	return 0;
}

int sigdelset(sigset_t *s, int sig)
{
	unsigned w; int b = sig_bit(sig, &w);
	if (b < 0) { errno = EINVAL; return -1; }
	s->__bits[w] &= ~(1UL << b);
	return 0;
}

int sigismember(const sigset_t *s, int sig)
{
	unsigned w; int b = sig_bit(sig, &w);
	if (b < 0) return 0;
	return (int)((s->__bits[w] >> b) & 1);
}

int sigprocmask(int how, const sigset_t *set, sigset_t *old)
{
	return (int)__ret(__syscall6(SYS_rt_sigprocmask, how, (long)set, (long)old, 8, 0, 0));
}

int kill(pid_t pid, int sig)
{
	return (int)__ret(__sys2(SYS_kill, pid, sig));
}

/* raise: single-threaded self-signal. Also resolves libgcc's __aeabi_*div0 -> raise(SIGFPE). */
int raise(int sig)
{
	return kill(getpid(), sig);
}

/* Kernel struct sigaction (ARM order): handler, flags, restorer, mask. */
struct k_sigaction {
	void (*handler)(int);
	unsigned long flags;
	void (*restorer)(void);
	sigset_t mask;
};

sighandler_t signal(int sig, sighandler_t handler)
{
	struct k_sigaction sa, old;
	memset(&sa, 0, sizeof sa);
	memset(&old, 0, sizeof old);
	sa.handler = handler;   /* SIG_DFL/SIG_IGN today; a catching handler needs SA_RESTORER (TODO) */
	if (__ret(__syscall6(SYS_rt_sigaction, sig, (long)&sa, (long)&old, 8, 0, 0)) < 0)
		return SIG_ERR;
	return old.handler;
}
