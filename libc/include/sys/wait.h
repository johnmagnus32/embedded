/*
 * sys/wait.h — wait for a child to change state, over wait4(2).
 *
 * Status-decoding macros match the kernel's encoding (proc.c): a normal exit
 * stores (code & 0xff) << 8; a kill-by-signal stores the signal in the low 7
 * bits. WEXITSTATUS/WIFEXITED/WTERMSIG mirror the standard POSIX bit layout.
 */
#ifndef _LIBC_SYS_WAIT_H
#define _LIBC_SYS_WAIT_H

#include <sys/types.h>

pid_t wait(int *status);
pid_t waitpid(pid_t pid, int *status, int options);

#define WEXITSTATUS(s) (((s) >> 8) & 0xff)
#define WTERMSIG(s)    ((s) & 0x7f)
#define WIFEXITED(s)   (WTERMSIG(s) == 0)
#define WIFSIGNALED(s) (WTERMSIG(s) != 0 && WTERMSIG(s) != 0x7f)

/* waitpid options (Linux ABI values). WNOHANG => return 0 instead of blocking when no
 * child has changed state — waitpid() forwards these to wait4(2). */
#define WNOHANG    1
#define WUNTRACED  2
#define WCONTINUED 8

#endif /* _LIBC_SYS_WAIT_H */
