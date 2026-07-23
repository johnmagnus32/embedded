/*
 * hello.c — the first program built against gv3libc (NOT hand-rolled syscalls).
 *
 * Proves the whole library end to end: crt0 -> __libc_start_main -> main() with
 * a real argc/argv/envp, printf() over write(), malloc()/free() over brk, and
 * exit(). If this prints correctly on our kernel, the libc's startup + syscall +
 * stdio + allocator paths all work.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv, char **envp)
{
	printf("hello: linked against gv3libc, pid %d\n", getpid());

	printf("hello: argc=%d\n", argc);
	for (int i = 0; i < argc; i++)
		printf("hello:   argv[%d] = %s\n", i, argv[i]);
	for (int i = 0; envp[i]; i++)
		printf("hello:   env: %s\n", envp[i]);

	/* exercise malloc/free */
	char *buf = malloc(64);
	if (buf) {
		strcpy(buf, "malloc/free works");
		printf("hello: %s\n", buf);
		free(buf);
	} else {
		printf("hello: malloc FAILED\n");
	}

	/* isatty on stdout */
	printf("hello: stdout %s a tty\n", isatty(1) ? "IS" : "is NOT");

	return 3;   /* a distinctive exit code the parent can wait4() on */
}
