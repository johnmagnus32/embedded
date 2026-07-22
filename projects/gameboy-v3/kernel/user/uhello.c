/*
 * uhello.c — the S8/S9/S10 child program, execve'd FROM THE FILESYSTEM.
 *
 * S10 upgrade: it now has a real crt-style entry (_ustart in asm) that reads the
 * initial stack the kernel built — argc at [sp], argv at sp+4, envp after the
 * argv NULL — exactly as musl's _start does. Printing argv/envp back proves the
 * kernel's argv/env/auxv stack layout is correct. Then it reads a file (VFS) and
 * exits 7 (harvested by init's wait4). Self-contained, no libc.
 */

#include <stdint.h>

#define SYS_exit    1
#define SYS_read    3
#define SYS_write   4
#define SYS_close   6
#define SYS_openat 322
#define AT_FDCWD  (-100)
#define O_RDONLY  0

static inline long usys(long nr, long a0, long a1, long a2, long a3)
{
	register long r7 __asm__("r7") = nr;
	register long r0 __asm__("r0") = a0;
	register long r1 __asm__("r1") = a1;
	register long r2 __asm__("r2") = a2;
	register long r3 __asm__("r3") = a3;
	__asm__ volatile("svc 0" : "+r"(r0) : "r"(r7),"r"(r1),"r"(r2),"r"(r3) : "memory");
	return r0;
}
static unsigned slen(const char *s){unsigned n=0;while(s[n])n++;return n;}
static void puts_(const char *s){usys(SYS_write,1,(long)s,slen(s),0);}

/* C body: receives the raw initial stack pointer (points at argc). */
__attribute__((used))
static void cmain(long *sp)
{
	long argc = sp[0];
	char **argv = (char **)&sp[1];
	char **envp = argv + argc + 1;          /* skip argv[] + its NULL */

	puts_("hello: I am /bin/hello, loaded from the ramfs.\n");
	puts_("hello: argc="); { char c='0'+(char)(argc%10); usys(SYS_write,1,(long)&c,1,0); } puts_("\n");
	for (long i = 0; i < argc; i++) {
		puts_("hello:   argv["); { char c='0'+(char)(i%10); usys(SYS_write,1,(long)&c,1,0); }
		puts_("] = "); puts_(argv[i]); puts_("\n");
	}
	for (int i = 0; envp[i]; i++) { puts_("hello:   env: "); puts_(envp[i]); puts_("\n"); }

	long fd = usys(SYS_openat, AT_FDCWD, (long)"/etc/motd", O_RDONLY, 0);
	if (fd >= 0) {
		char buf[128];
		long n = usys(SYS_read, fd, (long)buf, sizeof(buf), 0);
		if (n > 0) { puts_("hello: /etc/motd says: "); usys(SYS_write,1,(long)buf,n,0); }
		usys(SYS_close, fd, 0, 0, 0);
	}
	usys(SYS_exit, 7, 0, 0, 0);
	for (;;) {}
}

/* Entry: the kernel enters here with sp -> argc. Pass sp to cmain (don't touch
 * sp before capturing it). ARM: r0 = sp, then branch. */
__attribute__((naked, noreturn))
void _ustart(void)
{
	__asm__ volatile("mov r0, sp\n\t bl cmain\n\t" ::: "r0", "lr");
}
