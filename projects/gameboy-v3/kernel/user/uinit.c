/*
 * uinit.c — the S9 "init" program (PID 1), exercising the new filesystem.
 *
 * It proves the VFS end to end, all via real Linux syscalls (svc 0, nr in r7):
 *   1. writes to fd 1 (the console, opened for us by the kernel),
 *   2. openat()+read()s a data file (/etc/motd) out of the ramfs,
 *   3. getdents64()'s a directory (/) and prints the entries,
 *   4. fork()s; the child execve()s /bin/hello (a SEPARATE ELF loaded FROM the
 *      filesystem, not an embedded blob); the parent wait4()s for it.
 *
 * Seeing the file contents, the directory listing, and /bin/hello's output =
 * open/read/getdents/stat/execve-from-fs all working. Self-contained: no libc.
 */

#include <stdint.h>

#define SYS_exit         1
#define SYS_read         3
#define SYS_write        4
#define SYS_close        6
#define SYS_fork         2
#define SYS_execve      11
#define SYS_getpid      20
#define SYS_getdents64 217
#define SYS_openat     322
#define SYS_wait4      114
#define SYS_brk         45
#define SYS_mmap2      192
#define SYS_munmap      91
#define SYS_ioctl       54

#define AT_FDCWD  (-100)
#define O_RDONLY  0
#define PROT_RW    0x3
#define MAP_ANON_PRIV 0x22
#define TIOCGWINSZ 0x5413

static inline long usys(long nr, long a0, long a1, long a2, long a3)
{
	register long r7 __asm__("r7") = nr;
	register long r0 __asm__("r0") = a0;
	register long r1 __asm__("r1") = a1;
	register long r2 __asm__("r2") = a2;
	register long r3 __asm__("r3") = a3;
	__asm__ volatile("svc 0" : "+r"(r0) : "r"(r7), "r"(r1), "r"(r2), "r"(r3) : "memory");
	return r0;
}

static long uwrite(int fd, const void *s, unsigned n) { return usys(SYS_write, fd, (long)s, n, 0); }
static long uread(int fd, void *b, unsigned n)        { return usys(SYS_read, fd, (long)b, n, 0); }
static long uclose(int fd)                            { return usys(SYS_close, fd, 0, 0, 0); }
static long uopen(const char *p, int fl)              { return usys(SYS_openat, AT_FDCWD, (long)p, fl, 0); }
static long ugetdents(int fd, void *b, unsigned n)    { return usys(SYS_getdents64, fd, (long)b, n, 0); }
static long ufork(void)                               { return usys(SYS_fork, 0, 0, 0, 0); }
static long ugetpid(void)                             { return usys(SYS_getpid, 0, 0, 0, 0); }
static long uexecve(const char *p, char *const argv[], char *const envp[]) { return usys(SYS_execve, (long)p, (long)argv, (long)envp, 0); }
static long uwait4(int pid, int *st)                  { return usys(SYS_wait4, pid, (long)st, 0, 0); }
static long ubrk(unsigned long a)                     { return usys(SYS_brk, (long)a, 0, 0, 0); }
static long uioctl(int fd, unsigned long r, void *a)  { return usys(SYS_ioctl, fd, (long)r, (long)a, 0); }
static long umunmap(unsigned long a, unsigned n)      { return usys(SYS_munmap, (long)a, n, 0, 0); }
/* mmap2 needs 6 args (r0-r5); use a dedicated 6-arg asm wrapper. */
static long ummap2(unsigned long addr, unsigned len, int prot, int flags, int fd, unsigned pgoff)
{
	register long r7 __asm__("r7") = SYS_mmap2;
	register long r0 __asm__("r0") = (long)addr;
	register long r1 __asm__("r1") = len;
	register long r2 __asm__("r2") = prot;
	register long r3 __asm__("r3") = flags;
	register long r4 __asm__("r4") = fd;
	register long r5 __asm__("r5") = pgoff;
	__asm__ volatile("svc 0" : "+r"(r0)
	                 : "r"(r7),"r"(r1),"r"(r2),"r"(r3),"r"(r4),"r"(r5) : "memory");
	return r0;
}
__attribute__((noreturn))
static void uexit(int c)                              { usys(SYS_exit, c, 0, 0, 0); for (;;) {} }

static unsigned slen(const char *s) { unsigned n = 0; while (s[n]) n++; return n; }
static void puts_(const char *s) { uwrite(1, s, slen(s)); }
static void putint(int v)
{
	char b[12]; int i = 0;
	unsigned u = (v < 0) ? (unsigned)(-v) : (unsigned)v;
	if (v < 0) uwrite(1, "-", 1);
	do { b[i++] = (char)('0' + u % 10); u /= 10; } while (u);
	while (i) { i--; uwrite(1, &b[i], 1); }
}

/* struct linux_dirent64: d_ino(8) d_off(8) d_reclen(2) d_type(1) d_name[]@19 */
static void list_dir(const char *path)
{
	long fd = uopen(path, O_RDONLY);
	if (fd < 0) { puts_("init: open dir failed\n"); return; }
	char buf[512];
	long n = ugetdents(fd, buf, sizeof(buf));
	puts_("init: contents of ");
	puts_(path);
	puts_(":\n");
	long off = 0;
	while (off < n) {
		unsigned short reclen = *(unsigned short *)(buf + off + 16);
		unsigned char  dtype  = *(unsigned char  *)(buf + off + 18);
		const char *name = buf + off + 19;
		puts_("    ");
		puts_(name);
		puts_(dtype == 4 ? "/\n" : "\n");   /* DT_DIR=4 */
		if (reclen == 0) break;
		off += reclen;
	}
	uclose((int)fd);
}

static void cat_file(const char *path)
{
	long fd = uopen(path, O_RDONLY);
	if (fd < 0) { puts_("init: open file failed\n"); return; }
	puts_("init: contents of ");
	puts_(path);
	puts_(":\n    ");
	char buf[256];
	long n;
	while ((n = uread((int)fd, buf, sizeof(buf))) > 0)
		uwrite(1, buf, (unsigned)n);
	puts_("\n");
	uclose((int)fd);
}

/* S10: exercise the new memory syscalls + tty ioctl end to end. */
static void mem_test(void)
{
	puts_("init: --- S10 memory syscall test ---\n");

	/* brk: query current break, grow by 8 KiB, write+read-back through it. */
	unsigned long b0 = (unsigned long)ubrk(0);
	puts_("init: brk(0) = 0x"); { char h[9]; for(int i=7;i>=0;i--){int d=(b0>>(i*4))&0xf; h[7-i]=(char)(d<10?'0'+d:'a'+d-10);} h[8]=0; puts_(h); }
	puts_("\n");
	unsigned long b1 = (unsigned long)ubrk(b0 + 0x2000);
	if (b1 >= b0 + 0x2000) {
		volatile unsigned *heap = (volatile unsigned *)b0;
		heap[0] = 0xC0FFEE10; heap[1] = 0xBEEF0002;
		puts_(heap[0]==0xC0FFEE10 && heap[1]==0xBEEF0002
		      ? "init: brk grow + heap write/read OK\n"
		      : "init: brk heap READBACK MISMATCH\n");
	} else {
		puts_("init: brk grow FAILED\n");
	}

	/* mmap2: map one anon private page, use it, then unmap. */
	long p = ummap2(0, 4096, PROT_RW, MAP_ANON_PRIV, -1, 0);
	if (p > 0 && (unsigned long)p < 0xfffff000u) {
		volatile unsigned *m = (volatile unsigned *)p;
		m[0] = 0x5A5A1234; m[100] = 0xA5A5;
		puts_(m[0]==0x5A5A1234 && m[100]==0xA5A5
		      ? "init: mmap2 anon page write/read OK\n"
		      : "init: mmap2 page READBACK MISMATCH\n");
		puts_(umunmap((unsigned long)p, 4096) == 0
		      ? "init: munmap OK\n" : "init: munmap FAILED\n");
	} else {
		puts_("init: mmap2 FAILED\n");
	}

	/* tty: isatty() uses TIOCGWINSZ — must succeed on the console for a shell. */
	unsigned short ws[4] = {0,0,0,0};
	if (uioctl(1, TIOCGWINSZ, ws) == 0) {
		puts_("init: TIOCGWINSZ ok, "); putint(ws[0]); puts_("x"); putint(ws[1]);
		puts_(" (isatty path works)\n");
	} else {
		puts_("init: TIOCGWINSZ FAILED (shell would think stdout is not a tty)\n");
	}
}

__attribute__((noreturn))
void _ustart(void)
{
	puts_("init: hello from the filesystem, I am pid ");
	putint((int)ugetpid());
	puts_("\n");

	cat_file("/etc/motd");
	list_dir("/");
	list_dir("/bin");
	mem_test();

	long pid = ufork();
	if (pid == 0) {
		puts_("init: [child] execve(\"/bin/hello\") from the fs\n");
		char *av[] = { "/bin/hello", "arg1", 0 };
		char *ev[] = { "HOME=/", 0 };
		uexecve("/bin/hello", av, ev);
		puts_("init: [child] execve FAILED\n");
		uexit(127);
	}

	int status = 0;
	long w = uwait4((int)pid, &status);
	puts_("init: child pid ");
	putint((int)w);
	puts_(" exited with code ");
	putint((status >> 8) & 0xff);
	puts_("\n");
	puts_("init: S9 filesystem test complete, exiting 0\n");
	uexit(0);
}
