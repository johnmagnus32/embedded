/*
 * start.c — the C half of program startup. crt0.S captures the initial stack
 * pointer (aimed at argc) and calls us. We decode argc/argv/envp exactly as the
 * kernel laid them out (kernel/proc.c setup_user_stack), publish `environ`,
 * call main(), and _exit() with its return value.
 */
#include <unistd.h>

int   errno;                     /* the process-global errno (single-threaded) */
char **environ;                  /* the environment, per POSIX                 */

extern int main(int argc, char **argv, char **envp);

__attribute__((used, noreturn))
void __libc_start_main(long *initial_sp)
{
	int    argc = (int)initial_sp[0];
	char **argv = (char **)&initial_sp[1];
	char **envp = argv + argc + 1;          /* skip argv[] and its NULL         */

	environ = envp;
	int rc = main(argc, argv, envp);
	_exit(rc);
}
