/*
 * fs_syscall.c — the S9 file syscalls, implemented over ramfs + the fd table.
 *
 * Model: fd (per-process int) -> struct file (offset + flags, shared) -> inode.
 * The console (a char device at /dev/console) is special-cased: reads/writes to
 * it go to the UART, so a shell's stdin/stdout/stderr work. Everything else is
 * plain ramfs I/O.
 *
 * User pointers are read/written directly — during a syscall the caller's
 * address space is active (its L1 is in TTBR0), the same property S8 relies on.
 */

#include <stdint.h>
#include "fs_syscall.h"
#include "fs_abi.h"
#include "file.h"
#include "ramfs.h"
#include "vfs.h"
#include "proc.h"
#include "kstat.h"
#include "dirent.h"
#include "uart.h"
#include "libk.h"

static int console_echo(void);   /* defined below with the termios ioctls */

/* Is this inode the console char device? (major 5, minor 1 per the cpio node.) */
static int is_console(struct rf_inode *ino)
{
	return ino && ino->type == RF_CHR &&
	       ino->rdev_major == 5 && ino->rdev_minor == 1;
}

/* Resolve dirfd+path. S9 supports AT_FDCWD and absolute paths; a real dirfd
 * (open directory) is uncommon for a shell's core path and left as -EBADF. */
static struct rf_inode *resolve_at(int dirfd, const char *path, int follow)
{
	if (path[0] == '/')
		return vfs_resolve(0, path, follow);
	if (dirfd == AT_FDCWD)
		return vfs_resolve(proc_cwd(), path, follow);
	/* dirfd-relative: use the fd's inode as the base if it's a directory */
	struct file *f = fd_get(proc_fds(), dirfd);
	if (f && f->inode && f->inode->type == RF_DIR)
		return vfs_resolve(f->inode, path, follow);
	return 0;
}

/* Split "a/b/c" -> parent dir "a/b" + leaf "c" (for O_CREAT). Returns parent
 * inode (resolving symlinks on the way) or NULL; writes leaf name out. */
static struct rf_inode *parent_of(struct rf_inode *cwd, const char *path,
                                  char *leaf, uint32_t cap)
{
	/* find last '/' */
	int lastslash = -1;
	for (int i = 0; path[i]; i++) if (path[i] == '/') lastslash = i;
	if (lastslash < 0) {                     /* bare name in cwd */
		strlcpy_(leaf, path, cap);
		return cwd ? cwd : ramfs_root();
	}
	char dirpath[128];
	int n = lastslash == 0 ? 1 : lastslash;  /* keep root "/" as "/" */
	if (n > (int)sizeof(dirpath) - 1) n = sizeof(dirpath) - 1;
	memcpy(dirpath, path, n);
	dirpath[n] = '\0';
	strlcpy_(leaf, path + lastslash + 1, cap);
	return vfs_resolve(cwd, dirpath, 1);
}

long sys_openat(int dirfd, const char *path, int flags, uint32_t mode)
{
	/* POSIX: an empty pathname is -ENOENT, not the cwd/root dir. (Our resolver
	 * treats "" as "stay put" -> the start directory, which would hand back a
	 * usable fd on a directory. cttyhack relies on this failing: when it can't
	 * identify the console it open("")s and, on the -ENOENT, keeps the inherited
	 * console fds instead of dup2'ing a bogus one over stdin/out/err.) */
	if (!path || path[0] == '\0')
		return -K_ENOENT;
	struct rf_inode *ino = resolve_at(dirfd, path, !(flags & O_NOFOLLOW));

	if (!ino) {
		if (!(flags & O_CREAT))
			return -K_ENOENT;
		/* create a regular file in the parent directory */
		char leaf[64];
		struct rf_inode *dir = parent_of(proc_cwd(), path, leaf, sizeof(leaf));
		if (!dir || dir->type != RF_DIR)
			return -K_ENOENT;
		ino = ramfs_mkreg(dir, leaf, mode ? (mode & 07777) : 0644);
		if (!ino)
			return -K_ENOMEM;
	} else {
		if ((flags & O_DIRECTORY) && ino->type != RF_DIR)
			return -K_ENOTDIR;
		if ((flags & O_TRUNC) && ino->type == RF_REG)
			ramfs_truncate(ino, 0);
	}

	struct file *f = file_open_inode(ino, (uint32_t)flags);
	if (!f)
		return -K_ENOMEM;
	if ((flags & O_APPEND) && ino->type == RF_REG)
		f->off = ino->size;
	int fd = fd_alloc(proc_fds(), f);
	if (fd < 0) { file_close(f); return -K_EMFILE; }
	return fd;
}

long sys_close(int fd)
{
	return fd_close(proc_fds(), fd);
}

const void *read_block_chan(int fd, uint32_t n)
{
	if (n == 0)
		return 0;                            /* read(.,0) returns 0, never blocks */
	struct file *f = fd_get(proc_fds(), fd);
	if (!f || f->inode->type != RF_PIPE)
		return 0;                            /* only pipes block here */
	if (f->flags & O_NONBLOCK)
		return 0;                            /* O_NONBLOCK: return -EAGAIN/0, don't wait */
	/* Empty AND a writer still exists -> data may yet arrive; block on the pipe
	 * inode (sys_write wakes it). Empty with no writers -> EOF (don't block).
	 * Non-empty -> data ready (don't block). */
	if (f->inode->size == 0 && f->inode->wr_open > 0)
		return f->inode;                     /* wait channel = the pipe inode */
	return 0;
}

long sys_read(int fd, void *buf, uint32_t n)
{
	struct file *f = fd_get(proc_fds(), fd);
	if (!f)
		return -K_EBADF;
	if (is_console(f->inode)) {
		/* Blocking line-ish read from UART: read up to n bytes, stop at CR/LF.
		 * Echo only when the tty is in ECHO mode (canonical). A raw-mode line
		 * editor (BusyBox) clears ECHO and echoes itself; echoing here too would
		 * double every character. See console_echo() / the termios ioctls. */
		int echo = console_echo();
		uint8_t *d = buf;
		uint32_t i = 0;
		while (i < n) {
			int c = uart0_getc();            /* blocks for one char */
			if (c < 0) break;
			if (c == '\r') c = '\n';
			if (echo) uart0_putc(c == '\n' ? '\r' : (char)c);
			if (c == '\n') { if (echo) uart0_putc('\n'); d[i++] = '\n'; break; }
			d[i++] = (uint8_t)c;
		}
		return (long)i;
	}
	if (f->inode->type == RF_PIPE)
		return ramfs_pipe_read(f->inode, buf, n);   /* no offset for pipes */
	if (f->inode->type == RF_DIR)
		return -K_EISDIR;
	long r = ramfs_read(f->inode, f->off, buf, n);
	if (r > 0) f->off += (uint32_t)r;
	return r;
}

long sys_write(int fd, const void *buf, uint32_t n)
{
	struct file *f = fd_get(proc_fds(), fd);
	if (!f)
		return -K_EBADF;
	if (is_console(f->inode)) {
		const char *s = buf;
		for (uint32_t i = 0; i < n; i++) {
			if (s[i] == '\n') uart0_putc('\r');
			uart0_putc(s[i]);
		}
		return (long)n;
	}
	if (f->inode->type == RF_PIPE) {
		long r = ramfs_pipe_write(f->inode, buf, n);
		if (r > 0)
			proc_wakeup(f->inode);       /* wake readers blocked on this pipe */
		return r;
	}
	if (f->inode->type != RF_REG)
		return -K_EBADF;
	if (f->flags & O_APPEND)
		f->off = f->inode->size;
	long r = ramfs_write(f->inode, f->off, buf, n);
	if (r > 0) f->off += (uint32_t)r;
	return r;
}

/* iovec is {void* base; u32 len} on arm32 (8-byte stride) — verified. */
long sys_writev(int fd, uint32_t iov_uptr, int iovcnt)
{
	const uint32_t *iov = (const uint32_t *)(uintptr_t)iov_uptr;
	long total = 0;
	for (int i = 0; i < iovcnt; i++) {
		uint32_t base = iov[i * 2 + 0];
		uint32_t len  = iov[i * 2 + 1];
		if (!len) continue;
		long r = sys_write(fd, (const void *)(uintptr_t)base, len);
		if (r < 0) return total ? total : r;
		total += r;
		if ((uint32_t)r < len) break;        /* short write */
	}
	return total;
}

long sys_readv(int fd, uint32_t iov_uptr, int iovcnt)
{
	const uint32_t *iov = (const uint32_t *)(uintptr_t)iov_uptr;
	long total = 0;
	for (int i = 0; i < iovcnt; i++) {
		uint32_t base = iov[i * 2 + 0];
		uint32_t len  = iov[i * 2 + 1];
		if (!len) continue;
		long r = sys_read(fd, (void *)(uintptr_t)base, len);
		if (r < 0) return total ? total : r;
		total += r;
		if ((uint32_t)r < len) break;
	}
	return total;
}

/* seek to a 64-bit offset; we only support 32-bit magnitudes (files are small).
 * result is written back as a 64-bit loff_t to *result_uptr. */
long sys_llseek(int fd, uint32_t off_hi, uint32_t off_lo, uint32_t result_uptr, int whence)
{
	struct file *f = fd_get(proc_fds(), fd);
	if (!f)
		return -K_EBADF;
	if (off_hi != 0 && off_hi != 0xFFFFFFFFu)
		return -K_EINVAL;                    /* >4 GiB unsupported */
	int64_t base;
	switch (whence) {
	case SEEK_SET: base = 0; break;
	case SEEK_CUR: base = f->off; break;
	case SEEK_END: base = f->inode ? f->inode->size : 0; break;
	default: return -K_EINVAL;
	}
	int64_t delta = (int32_t)off_lo;         /* sign-extend low word */
	int64_t np = base + delta;
	if (np < 0)
		return -K_EINVAL;
	f->off = (uint32_t)np;
	if (result_uptr) {
		uint32_t *res = (uint32_t *)(uintptr_t)result_uptr;
		res[0] = (uint32_t)np;               /* lo */
		res[1] = 0;                          /* hi */
	}
	return 0;
}

long sys_lseek(int fd, int32_t off, int whence)
{
	struct file *f = fd_get(proc_fds(), fd);
	if (!f)
		return -K_EBADF;
	int64_t base;
	switch (whence) {
	case SEEK_SET: base = 0; break;
	case SEEK_CUR: base = f->off; break;
	case SEEK_END: base = f->inode ? f->inode->size : 0; break;
	default: return -K_EINVAL;
	}
	int64_t np = base + off;
	if (np < 0) return -K_EINVAL;
	f->off = (uint32_t)np;
	return (long)np;
}

long sys_dup(int fd)
{
	struct file *f = fd_get(proc_fds(), fd);
	if (!f)
		return -K_EBADF;
	int nfd = fd_alloc(proc_fds(), file_dup(f));
	if (nfd < 0) { file_close(f); return -K_EMFILE; }
	return nfd;
}

/* pipe2(int fds[2], flags): create a pipe; fds[0]=read end, fds[1]=write end.
 * One RF_PIPE inode shared by two struct files (O_RDONLY / O_WRONLY). flags
 * (O_CLOEXEC/O_NONBLOCK) are accepted but not tracked. pipe(2) = pipe2(fds,0). */
long sys_pipe2(uint32_t fds_uptr, int flags)
{
	(void)flags;
	if (!fds_uptr) return -K_EINVAL;
	struct rf_inode *p = ramfs_pipe();
	if (!p) return -K_ENOMEM;
	struct file *rf = file_open_inode(p, O_RDONLY);
	struct file *wf = file_open_inode(p, O_WRONLY);
	if (!rf || !wf) { if (rf) file_close(rf); if (wf) file_close(wf); return -K_ENOMEM; }
	int rfd = fd_alloc(proc_fds(), rf);
	int wfd = fd_alloc(proc_fds(), wf);
	if (rfd < 0 || wfd < 0) {
		if (rfd >= 0) fd_close(proc_fds(), rfd); else file_close(rf);
		if (wfd >= 0) fd_close(proc_fds(), wfd); else file_close(wf);
		return -K_EMFILE;
	}
	uint32_t *out = (uint32_t *)(uintptr_t)fds_uptr;
	out[0] = (uint32_t)rfd;
	out[1] = (uint32_t)wfd;
	return 0;
}

long sys_dup2(int oldfd, int newfd)
{
	struct file *f = fd_get(proc_fds(), oldfd);
	if (!f)
		return -K_EBADF;
	if (oldfd == newfd)
		return newfd;
	if (newfd < 0 || newfd >= NOFILE)
		return -K_EBADF;                     /* reject before dup'ing (no leak) */
	return fd_install_at(proc_fds(), newfd, file_dup(f));
}

long sys_fcntl64(int fd, int cmd, uint32_t arg)
{
	struct file *f = fd_get(proc_fds(), fd);
	if (!f)
		return -K_EBADF;
	switch (cmd) {
	case F_DUPFD:
	case F_DUPFD_CLOEXEC: {
		int nfd = fd_alloc_from(proc_fds(), file_dup(f), (int)arg);
		if (nfd < 0) { file_close(f); return -K_EMFILE; }
		return nfd;
	}
	case F_GETFD: return 0;                  /* no per-fd CLOEXEC tracking yet */
	case F_SETFD: return 0;
	case F_GETFL: return (long)f->flags;
	case F_SETFL: f->flags = arg; return 0;
	default: return -K_EINVAL;
	}
}

long sys_getdents64(int fd, uint32_t buf_uptr, uint32_t cap)
{
	struct file *f = fd_get(proc_fds(), fd);
	if (!f)
		return -K_EBADF;
	struct rf_inode *dir = f->inode;
	if (!dir || dir->type != RF_DIR)
		return -K_ENOTDIR;

	uint8_t *out = (uint8_t *)(uintptr_t)buf_uptr;
	uint32_t used = 0;

	/* Build a synthetic ordered list: "." , ".." , then children. We track the
	 * cursor in f->dir_pos (0=".", 1="..", 2+ = nth child). */
	for (;;) {
		const char *name = 0;
		struct rf_inode *tgt = 0;
		if (f->dir_pos == 0)      { name = ".";  tgt = dir; }
		else if (f->dir_pos == 1) { name = ".."; tgt = dir->parent; }
		else {
			uint32_t idx = f->dir_pos - 2, k = 0;
			for (struct rf_dirent *d = dir->children; d; d = d->next, k++)
				if (k == idx) { name = d->name; tgt = d->inode; break; }
			if (!name) break;                /* end of directory */
		}

		uint32_t namelen = (uint32_t)strlen(name);
		uint32_t reclen = DIRENT64_RECLEN(namelen);
		if (used + reclen > cap) {
			if (used == 0)
				return -K_EINVAL;            /* buffer too small for even 1 entry */
			break;                           /* buffer full — stop (musl re-calls) */
		}

		struct linux_dirent64 *de = (struct linux_dirent64 *)(out + used);
		de->d_ino = tgt ? tgt->ino : 0;
		de->d_off = (int64_t)(used + reclen); /* cookie: offset of next record */
		de->d_reclen = (uint16_t)reclen;
		de->d_type = tgt ? (tgt->type == RF_DIR ? DT_DIR :
		                     tgt->type == RF_LNK ? DT_LNK :
		                     tgt->type == RF_CHR ? DT_CHR : DT_REG)
		                 : DT_UNKNOWN;
		strcpy(de->d_name, name);
		/* zero the pad between name+NUL and reclen */
		for (uint32_t z = DIRENT64_NAMEOFF + namelen + 1; z < reclen; z++)
			((uint8_t *)de)[z] = 0;

		used += reclen;
		f->dir_pos++;
	}
	return (long)used;
}

/* Fill a verified ARM struct kstat64 for `ino`. */
static void fill_stat(struct rf_inode *ino, struct kstat64 *st)
{
	memset(st, 0, sizeof(*st));
	st->st_dev    = 1;                       /* fake device id */
	st->__st_ino  = ino->ino;
	st->st_ino    = ino->ino;
	st->st_mode   = ino->mode;
	st->st_nlink  = ino->nlink;
	st->st_uid    = 0;
	st->st_gid    = 0;
	st->st_size   = (int64_t)ino->size;
	st->st_blksize = 4096;
	st->st_blocks = (ino->size + 511) / 512;
	if (ino->type == RF_CHR)
		st->st_rdev = ((uint64_t)ino->rdev_major << 8) | ino->rdev_minor;
}

long sys_fstat64(int fd, uint32_t statbuf_uptr)
{
	struct file *f = fd_get(proc_fds(), fd);
	if (!f)
		return -K_EBADF;
	fill_stat(f->inode, (struct kstat64 *)(uintptr_t)statbuf_uptr);
	return 0;
}

long sys_stat64(const char *path, uint32_t statbuf_uptr, int follow)
{
	struct rf_inode *ino = vfs_resolve(proc_cwd(), path, follow);
	if (!ino)
		return -K_ENOENT;
	fill_stat(ino, (struct kstat64 *)(uintptr_t)statbuf_uptr);
	return 0;
}

long sys_fstatat64(int dirfd, const char *path, uint32_t statbuf_uptr, int flags)
{
	int follow = !(flags & AT_SYMLINK_NOFOLLOW);
	struct rf_inode *ino;
	if ((flags & AT_EMPTY_PATH) && (!path || !path[0])) {
		struct file *f = fd_get(proc_fds(), dirfd);
		if (!f) return -K_EBADF;
		ino = f->inode;
	} else {
		ino = resolve_at(dirfd, path, follow);
	}
	if (!ino)
		return -K_ENOENT;
	fill_stat(ino, (struct kstat64 *)(uintptr_t)statbuf_uptr);
	return 0;
}

/* statx(dirfd, path, flags, mask, buf): modern stat. struct statx field offsets
 * (linux/stat.h): stx_mask@0, stx_blksize@4, stx_attributes@8(u64), stx_nlink@16,
 * stx_uid@20, stx_gid@24, stx_mode@28(u16), stx_ino@32(u64), stx_size@40(u64),
 * stx_blocks@48(u64). We fill the fields busybox reads and set stx_mask to the
 * basic-stats bits (STATX_BASIC_STATS=0x7ff). AT_EMPTY_PATH -> stat the dirfd. */
long sys_statx(int dirfd, const char *path, int flags, uint32_t mask, uint32_t buf_uptr)
{
	(void)mask;
	struct rf_inode *ino;
	if ((flags & AT_EMPTY_PATH) && (!path || !path[0])) {
		struct file *f = fd_get(proc_fds(), dirfd);
		if (!f) return -K_EBADF;
		ino = f->inode;
	} else {
		ino = resolve_at(dirfd, path, !(flags & AT_SYMLINK_NOFOLLOW));
	}
	if (!ino) return -K_ENOENT;
	if (!buf_uptr) return -K_EINVAL;

	uint8_t *b = (uint8_t *)(uintptr_t)buf_uptr;
	memset(b, 0, 256);                       /* struct statx is 256 bytes */
	#define W32(o,v) (*(uint32_t *)(b + (o)) = (uint32_t)(v))
	#define W16(o,v) (*(uint16_t *)(b + (o)) = (uint16_t)(v))
	#define W64(o,v) (*(uint64_t *)(b + (o)) = (uint64_t)(v))
	W32(0, 0x7ff);                           /* stx_mask = STATX_BASIC_STATS */
	W32(4, 4096);                            /* stx_blksize */
	W32(16, ino->nlink);                     /* stx_nlink */
	W32(20, 0); W32(24, 0);                  /* stx_uid / stx_gid = root */
	W16(28, ino->mode);                      /* stx_mode */
	W64(32, ino->ino);                       /* stx_ino */
	W64(40, ino->size);                      /* stx_size */
	W64(48, (ino->size + 511) / 512);        /* stx_blocks */
	#undef W32
	#undef W16
	#undef W64
	return 0;
}

long sys_faccessat(int dirfd, const char *path, int mode)
{
	(void)mode;
	struct rf_inode *ino = resolve_at(dirfd, path, 1);
	return ino ? 0 : -K_ENOENT;              /* everything is accessible (root) */
}

long sys_readlinkat(int dirfd, const char *path, uint32_t buf_uptr, uint32_t bufsz)
{
	struct rf_inode *ino = resolve_at(dirfd, path, 0);   /* don't follow final */
	if (!ino)
		return -K_ENOENT;
	if (ino->type != RF_LNK)
		return -K_EINVAL;
	uint32_t n = ino->size < bufsz ? ino->size : bufsz;
	memcpy((void *)(uintptr_t)buf_uptr, ino->target, n);
	return (long)n;                          /* readlink does NOT NUL-terminate */
}

long sys_chdir(const char *path)
{
	struct rf_inode *ino = vfs_resolve(proc_cwd(), path, 1);
	if (!ino)
		return -K_ENOENT;
	if (ino->type != RF_DIR)
		return -K_ENOTDIR;
	proc_set_cwd(ino);
	return 0;
}

/* Reconstruct the absolute path of the cwd by walking parent links. */
long sys_getcwd(uint32_t buf_uptr, uint32_t size)
{
	char tmp[128];
	uint32_t pos = sizeof(tmp);
	tmp[--pos] = '\0';
	struct rf_inode *ino = proc_cwd();
	struct rf_inode *rootn = ramfs_root();

	if (ino == rootn) {
		if (pos == 0) return -K_ERANGE;
		tmp[--pos] = '/';
	}
	int guard = 0;
	while (ino && ino != rootn) {
		if (++guard > 64) return -K_EINVAL;   /* broken parent chain — bail */
		/* find our name in the parent's child list */
		const char *name = "?";
		for (struct rf_dirent *d = ino->parent->children; d; d = d->next)
			if (d->inode == ino) { name = d->name; break; }
		uint32_t nl = (uint32_t)strlen(name);
		if (pos < nl + 1) return -K_ERANGE;
		for (uint32_t i = 0; i < nl; i++) tmp[pos - nl + i] = name[i];
		pos -= nl;
		tmp[--pos] = '/';
		ino = ino->parent;
	}

	uint32_t len = sizeof(tmp) - pos;         /* includes NUL */
	if (len > size)
		return -K_ERANGE;
	memcpy((void *)(uintptr_t)buf_uptr, tmp + pos, len);
	return (long)len;                         /* getcwd returns the length */
}

/* tty ioctls: the request codes (TCGETS/TIOCGWINSZ/...), NCCS, and the termios
 * struct layout now come from the shared UAPI (uapi/gv3_abi.h, via fs_abi.h) so
 * the kernel and the rootfs libc agree byte-for-byte. CRITICAL: musl's isatty()
 * calls TIOCGWINSZ (NOT TCGETS), so that ioctl succeeding on fd 0/1 is what makes
 * the shell take its INTERACTIVE path. We alias the historical kernel names. */
#define NCCS      GV3_NCCS
#define k_termios gv3_termios      /* `struct k_termios` == `struct gv3_termios` */

/* termios flag bits we set so cooked-mode line handling looks sane to BusyBox.
 * Values are the ARM/asm-generic termbits octals (verified): c_lflag ISIG=0x1,
 * ICANON=0x2, ECHO=0x8; NOT the ECHOPRT(0x400)/etc. bits. */
#define K_ISIG   0000001   /* c_lflag */
#define K_ICANON 0000002   /* c_lflag — canonical (line) mode */
#define K_ECHO   0000010   /* c_lflag */
#define K_ICRNL  0000400   /* c_iflag: map CR->NL on input */
#define K_OPOST  0000001   /* c_oflag: output post-processing */
#define K_ONLCR  0000004   /* c_oflag: map NL->CRNL on output */
#define K_CS8    0000060   /* c_cflag */
#define K_CREAD  0000200   /* c_cflag */
#define K_B115200 0010002  /* c_cflag baud */

/* The console is a single shared device, so its line discipline is one global
 * termios. It starts in the canonical + echo + signals state a fresh tty has;
 * a program (e.g. BusyBox's line editor) can switch it to raw mode via TCSETS,
 * and the read path honors the current ECHO bit — see console_echo() / sys_read.
 * Without this the kernel echoes AND the shell's editor echoes => double chars. */
static struct k_termios g_console_tios;
static int g_console_tios_init;

static void console_tios_default(struct k_termios *t)
{
	memset(t, 0, sizeof(*t));
	t->c_iflag = K_ICRNL;
	t->c_oflag = K_OPOST | K_ONLCR;
	t->c_cflag = K_B115200 | K_CS8 | K_CREAD;
	t->c_lflag = K_ICANON | K_ECHO | K_ISIG;
	/* a couple of the standard control chars (VINTR=^C@0, VEOF=^D@4, VERASE=^?@2) */
	t->c_cc[0] = 3;      /* VINTR  = Ctrl-C */
	t->c_cc[2] = 0177;   /* VERASE = DEL    */
	t->c_cc[4] = 4;      /* VEOF   = Ctrl-D */
}

static struct k_termios *console_tios(void)
{
	if (!g_console_tios_init) {
		console_tios_default(&g_console_tios);
		g_console_tios_init = 1;
	}
	return &g_console_tios;
}

/* Should the kernel echo input chars on the console? Only in canonical ECHO
 * mode; a raw-mode line editor clears ECHO and echoes itself. */
static int console_echo(void)
{
	return (console_tios()->c_lflag & K_ECHO) != 0;
}

long sys_ioctl(int fd, uint32_t req, uint32_t arg)
{
	struct file *f = fd_get(proc_fds(), fd);
	if (!f)
		return -K_EBADF;
	int tty = is_console(f->inode);
	switch (req) {
	case TIOCGWINSZ:
		/* THE make-or-break for interactivity: musl isatty() lives here. */
		if (!tty) return -25;                /* -ENOTTY */
		if (arg) {
			uint16_t *ws = (uint16_t *)(uintptr_t)arg;  /* row, col, xpixel, ypixel */
			ws[0] = 24; ws[1] = 80; ws[2] = 0; ws[3] = 0;
		}
		return 0;
	case TCGETS:
		if (!tty) return -25;
		if (arg) *(struct k_termios *)(uintptr_t)arg = *console_tios();
		return 0;
	case TCSETS: case TCSETSW: case TCSETSF:
		/* Record the new line discipline so the read path honors ECHO/ICANON.
		 * This is what lets a raw-mode line editor turn off kernel echo. */
		if (!tty) return -25;
		if (arg) *console_tios() = *(const struct k_termios *)(uintptr_t)arg;
		return 0;
	case TIOCSWINSZ:
	case TIOCSCTTY: case TIOCSPGRP: case TIOCNOTTY:
		return tty ? 0 : -25;
	case TIOCGPGRP:
		if (!tty) return -25;
		if (arg) *(uint32_t *)(uintptr_t)arg = 1;   /* pretend pgrp 1 */
		return 0;
	case FIONREAD:
		if (arg) *(uint32_t *)(uintptr_t)arg = 0;   /* no buffered input */
		return tty ? 0 : -25;
	default:
		return -25;                          /* -ENOTTY */
	}
}
