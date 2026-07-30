/*
 * unistd.h — core POSIX process/file calls (subset gv3libc implements).
 * Signatures match POSIX so the same source can also build against musl.
 */
#ifndef _GV3_UNISTD_H
#define _GV3_UNISTD_H

#include <stddef.h>
#include <sys/types.h>
#include <gv3_abi.h>   /* SEEK_SET/CUR/END (shared UAPI) */

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

ssize_t read(int fd, void *buf, size_t n);
ssize_t write(int fd, const void *buf, size_t n);
int     close(int fd);
off_t   lseek(int fd, off_t off, int whence);

pid_t   fork(void);
int     execve(const char *path, char *const argv[], char *const envp[]);
int     execv(const char *path, char *const argv[]);
pid_t   getpid(void);
void    _exit(int status) __attribute__((noreturn));

int     chdir(const char *path);
char   *getcwd(char *buf, size_t size);

int     isatty(int fd);

void   *sbrk(long incr);

#endif /* _GV3_UNISTD_H */
