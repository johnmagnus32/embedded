/*
 * signal.h — signal sets, disposition, mask, and kill (subset).
 *
 * sigset_t is the 64-bit rt sigset the kernel's rt_* syscalls expect (sigsetsize = 8).
 * signal() goes over rt_sigaction; today only SIG_IGN/SIG_DFL are used (init), which need
 * no return trampoline — a real catching handler would need an SA_RESTORER (added later).
 */
#ifndef _LIBC_SIGNAL_H
#define _LIBC_SIGNAL_H

#include <sys/types.h>

typedef struct { unsigned long __bits[2]; } sigset_t;   /* 64 bits, ARM */
typedef void (*sighandler_t)(int);

#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)-1)

#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

#define SIGHUP   1
#define SIGINT   2
#define SIGQUIT  3
#define SIGILL   4
#define SIGABRT  6
#define SIGFPE   8
#define SIGKILL  9
#define SIGUSR1  10
#define SIGSEGV  11
#define SIGUSR2  12
#define SIGPIPE  13
#define SIGALRM  14
#define SIGTERM  15
#define SIGCHLD  17
#define SIGCONT  18
#define SIGSTOP  19
#define SIGTSTP  20

int sigemptyset(sigset_t *set);
int sigfillset(sigset_t *set);
int sigaddset(sigset_t *set, int sig);
int sigdelset(sigset_t *set, int sig);
int sigismember(const sigset_t *set, int sig);
int sigprocmask(int how, const sigset_t *set, sigset_t *old);
sighandler_t signal(int sig, sighandler_t handler);
int kill(pid_t pid, int sig);
int raise(int sig);

#endif /* _LIBC_SIGNAL_H */
