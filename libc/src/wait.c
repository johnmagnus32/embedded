/*
 * wait.c — wait()/waitpid() over the kernel's wait4(2).
 *
 * wait4(pid, status, options, rusage). We FORWARD `options` (e.g. WNOHANG) to the
 * kernel: a kernel that doesn't implement them ignores them (harmless), and a mainline
 * kernel honors them — so a non-blocking reap loop (waitpid(-1,…,WNOHANG)) works there.
 * rusage is passed as NULL (no accounting). pid == -1 means "any child".
 */
#include <sys/wait.h>
#include "syscall_internal.h"

pid_t waitpid(pid_t pid, int *status, int options)
{
	return (pid_t)__ret(__syscall6(SYS_wait4, pid, (long)status, options, 0, 0, 0));
}

pid_t wait(int *status)
{
	return waitpid(-1, status, 0);
}
