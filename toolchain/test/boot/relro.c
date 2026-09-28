/*
 * relro.c — a -fPIC dynamic program checking RELRO, booted as PID 1 by boot-dynamic.sh. `names` is a const table of
 * pointers: -fPIC puts it in .data.rel.ro (the loader fixes up its addresses), which ld covers with PT_GNU_RELRO and
 * our ld.so write-protects once relocating is done. A child that writes it must die of SIGSEGV.
 */
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

const char *const names[] = { "alpha", "beta" };

int main(void)
{
	pid_t pid = fork();
	if (pid == 0) { *(const char *volatile *)&names[0] = "gamma"; _exit(0); }   /* must fault */
	int st = 0;
	if (pid < 0 || waitpid(pid, &st, 0) != pid) { puts("relro: fork/waitpid failed"); return 1; }
	if (!WIFSIGNALED(st) || WTERMSIG(st) != 11) { printf("relro: the write succeeded (status %d): not protected\n", st); return 1; }
	printf("relro: .data.rel.ro is read-only after relocation (%s %s)\n", names[0], names[1]);
	return 0;
}
