/*
 * unistd.h — core POSIX process/file calls (subset libc implements).
 * Signatures match POSIX so the same source can also build against musl.
 */
#ifndef _LIBC_UNISTD_H
#define _LIBC_UNISTD_H

#include <stddef.h>
#include <sys/types.h>
#include <abi.h>   /* SEEK_SET/CUR/END (shared UAPI) */

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* access() mode bits */
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

ssize_t read(int fd, void *buf, size_t n);
ssize_t write(int fd, const void *buf, size_t n);
int     close(int fd);
off_t   lseek(int fd, off_t off, int whence);
int     dup2(int oldfd, int newfd);

pid_t   fork(void);
int     execve(const char *path, char *const argv[], char *const envp[]);
int     execv(const char *path, char *const argv[]);
int     execvp(const char *file, char *const argv[]);
pid_t   getpid(void);
void    _exit(int status) __attribute__((noreturn));

int     chdir(const char *path);
char   *getcwd(char *buf, size_t size);
int     access(const char *path, int mode);
int     unlink(const char *path);
int     rmdir(const char *path);
int     symlink(const char *target, const char *linkpath);
void    sync(void);

int     isatty(int fd);

void   *sbrk(long incr);

#endif /* _LIBC_UNISTD_H */
