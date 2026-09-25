/* rt.c — the bare-metal "kernel" our libc runs on for the GCC c-torture harness (qemu-system-arm -M virt +
 * semihosting). libc/src (string/stdlib/stdio/printf/malloc/lldiv) is linked unchanged on top; this file supplies
 * only the syscall layer it calls: write -> PL011 UART, sbrk -> a static arena, _exit -> semihosting
 * SYS_EXIT_EXTENDED (so the test's exit status becomes qemu's), and harmless stubs for the file calls.
 * Compiled by OUR cc for our runs and by GCC for the oracle runs, so both sides share one runtime. */
typedef unsigned long size_t;
typedef long ssize_t;

int errno;                /* libc state normally defined by start.c/syscall.c (not linked here) */
char **environ;

static void uart_putc(char c) { *(volatile unsigned char *)0x09000000 = (unsigned char)c; }

ssize_t write(int fd, const void *buf, size_t n) {
	(void)fd;
	const char *p = buf;
	for (size_t i = 0; i < n; i++) uart_putc(p[i]);
	return (ssize_t)n;
}
ssize_t read(int fd, void *buf, size_t n) { (void)fd; (void)buf; (void)n; return 0; }
int open(const char *path, int flags, ...) { (void)path; (void)flags; return -1; }
int close(int fd) { (void)fd; return 0; }
long lseek(int fd, long off, int whence) { (void)fd; (void)off; (void)whence; return -1; }

static char heap[4 << 20];   /* 4 MiB arena for malloc */
static size_t brk_off;
void *sbrk(long inc) {
	if (inc < 0 || brk_off + (size_t)inc > sizeof heap) return (void *)-1;
	void *p = heap + brk_off; brk_off += (size_t)inc; return p;
}

void _exit(int status) {
	/* ARM semihosting SYS_EXIT_EXTENDED: r0 = 0x20, r1 -> { ADP_Stopped_ApplicationExit, status } */
	unsigned block[2];
	block[0] = 0x20026u; block[1] = (unsigned)status;
	register unsigned op __asm__("r0") = 0x20;
	register unsigned *arg __asm__("r1") = block;
	__asm__ volatile("svc 0x123456" : : "r"(op), "r"(arg) : "memory");
	for (;;) ;
}

/* libgcc's __aeabi_idiv0 (divide by zero) raises SIGFPE; a signal here just ends the test like a crash would. */
int raise(int sig) { _exit(128 + sig); return 0; }
