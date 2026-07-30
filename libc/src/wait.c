/*
 * wait.c — wait()/waitpid() over the kernel's wait4(2).
 *
 * wait4(pid, status, options, rusage). Our kernel ignores options+rusage (no
 * WNOHANG / no rusage accounting yet), so we pass 0 for both. pid == -1 means
 * "any child" (our kernel's proc_wait harvests any zombie child of the caller).
 */
#include <sys/wait.h>
#include "syscall_internal.h"

pid_t waitpid(pid_t pid, int *status, int options)
{
	(void)options;   /* our kernel doesn't implement WNOHANG etc. yet */
	return (pid_t)__ret(__syscall6(SYS_wait4, pid, (long)status, 0, 0, 0, 0));
}

pid_t wait(int *status)
{
	return waitpid(-1, status, 0);
}
