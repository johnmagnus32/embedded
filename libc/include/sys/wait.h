/*
 * sys/wait.h — wait for a child to change state, over wait4(2).
 *
 * Status-decoding macros match the kernel's encoding (proc.c): a normal exit
 * stores (code & 0xff) << 8; a kill-by-signal stores the signal in the low 7
 * bits. WEXITSTATUS/WIFEXITED/WTERMSIG mirror the standard POSIX bit layout.
 */
#ifndef _GV3_SYS_WAIT_H
#define _GV3_SYS_WAIT_H

#include <sys/types.h>

pid_t wait(int *status);
pid_t waitpid(pid_t pid, int *status, int options);

#define WEXITSTATUS(s) (((s) >> 8) & 0xff)
#define WTERMSIG(s)    ((s) & 0x7f)
#define WIFEXITED(s)   (WTERMSIG(s) == 0)
#define WIFSIGNALED(s) (WTERMSIG(s) != 0 && WTERMSIG(s) != 0x7f)

#endif /* _GV3_SYS_WAIT_H */
