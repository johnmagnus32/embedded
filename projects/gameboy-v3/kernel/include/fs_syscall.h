/*
 * fs_syscall.h — the S9 file-related syscall handlers.
 *
 * Each takes raw register args (already read from the trapframe) and returns
 * the value the user sees in r0 (negative = -errno). They operate on the
 * CURRENT process's fd table + cwd (via proc_fds()/proc_cwd()). User pointers
 * are dereferenced directly: during a syscall the caller's address space is
 * active (TTBR0 -> its L1), exactly as S8's write/wait already rely on.
 */
#ifndef GV3K_FS_SYSCALL_H
#define GV3K_FS_SYSCALL_H

#include <stdint.h>

long sys_openat(int dirfd, const char *path, int flags, uint32_t mode);
long sys_close(int fd);
long sys_read(int fd, void *buf, uint32_t n);
/* If a read(fd, ., n) would block, return the wait channel to sleep on (the pipe
 * inode); else NULL. Blocks only for an empty pipe whose write end is still open
 * (a `$(...)` writer hasn't run yet). The dispatcher sleeps the reader on this
 * channel; sys_write wakes it. */
const void *read_block_chan(int fd, uint32_t n);
long sys_write(int fd, const void *buf, uint32_t n);
long sys_writev(int fd, uint32_t iov_uptr, int iovcnt);
long sys_readv(int fd, uint32_t iov_uptr, int iovcnt);
long sys_llseek(int fd, uint32_t off_hi, uint32_t off_lo, uint32_t result_uptr, int whence);
long sys_lseek(int fd, int32_t off, int whence);
long sys_dup(int fd);
long sys_pipe2(uint32_t fds_uptr, int flags);
long sys_dup2(int oldfd, int newfd);
long sys_fcntl64(int fd, int cmd, uint32_t arg);
long sys_getdents64(int fd, uint32_t buf_uptr, uint32_t cap);
long sys_fstat64(int fd, uint32_t statbuf_uptr);
long sys_stat64(const char *path, uint32_t statbuf_uptr, int follow);
long sys_fstatat64(int dirfd, const char *path, uint32_t statbuf_uptr, int flags);
long sys_statx(int dirfd, const char *path, int flags, uint32_t mask, uint32_t buf_uptr);
long sys_faccessat(int dirfd, const char *path, int mode);
long sys_readlinkat(int dirfd, const char *path, uint32_t buf_uptr, uint32_t bufsz);
long sys_chdir(const char *path);
long sys_getcwd(uint32_t buf_uptr, uint32_t size);
long sys_ioctl(int fd, uint32_t req, uint32_t arg);

#endif /* GV3K_FS_SYSCALL_H */
