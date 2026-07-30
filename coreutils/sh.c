/*
 * sh — a minimal shell for the gv3 rootfs. Runs either interactively (reading
 * lines from stdin with a prompt) or as a SCRIPT interpreter when given a file
 * argument — which is how the kernel's shebang handoff invokes us for a
 * `#!/bin/sh` /init: argv = ["/bin/sh", "/init"].
 *
 * Per line: skip blanks and `#` comments, split on whitespace into argv, then:
 *   - builtins in-process: `cd`, `exit`, `pwd`, `exec`
 *   - otherwise fork() + execve() the command, parent wait()s.
 * `exec CMD` replaces the shell itself (execve without fork) — lets a script
 * hand off to an interactive shell (`exec /bin/sh`) as the reference init does.
 * Command lookup: a path with '/' is used directly; a bare name is tried under
 * a small PATH ("/bin", then "."). No pipes/redirection/globbing/quoting yet —
 * uses ONLY syscalls our kernel implements.
 */
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MAXARGS 32
#define LINELEN 256

static const char *PATH_DIRS[] = { "/bin", ".", 0 };
static char **g_envp;              /* inherited env, used by exec/child */
static int   g_interactive;        /* print a prompt only when interactive */

/* read one line from `fd` into buf (NUL-terminated, newline stripped).
 * returns line length, or -1 on EOF with nothing read. Byte-at-a-time so it
 * works identically on the console (fd 0) and a script file. */
static int read_line(int fd, char *buf, int cap)
{
	int n = 0;
	for (;;) {
		char c;
		ssize_t r = read(fd, &c, 1);
		if (r <= 0)
			return (n > 0) ? n : -1;    /* EOF */
		if (c == '\n') { buf[n] = '\0'; return n; }
		if (c == '\b' || c == 0x7f) {       /* crude backspace */
			if (n > 0) n--;
			continue;
		}
		if (n < cap - 1)
			buf[n++] = c;
	}
}

/* split line into argv[] on spaces/tabs (in-place NUL-termination). */
static int tokenize(char *line, char **argv, int maxargs)
{
	int argc = 0;
	char *p = line;
	while (*p && argc < maxargs - 1) {
		while (*p == ' ' || *p == '\t') *p++ = '\0';
		if (!*p) break;
		argv[argc++] = p;
		while (*p && *p != ' ' && *p != '\t') p++;
	}
	argv[argc] = 0;
	return argc;
}

/* Resolve argv[0] and execve it (searching PATH for bare names). Never returns
 * on success; on total failure prints and returns to the caller. Used both by
 * the forked child path and the `exec` builtin (which runs it unforked). */
static void try_execve(char **argv)
{
	if (strchr(argv[0], '/')) {
		execve(argv[0], argv, g_envp);      /* explicit path */
	} else {
		char full[128];
		for (int i = 0; PATH_DIRS[i]; i++) {
			int n = 0;
			for (const char *d = PATH_DIRS[i]; *d && n < (int)sizeof(full)-1; d++) full[n++] = *d;
			if (n && full[n-1] != '/' && n < (int)sizeof(full)-1) full[n++] = '/';
			for (const char *c = argv[0]; *c && n < (int)sizeof(full)-1; c++) full[n++] = *c;
			full[n] = '\0';
			execve(full, argv, g_envp);     /* returns only on failure */
		}
	}
}

/* Execute one already-tokenized command line. Returns 1 to keep looping, 0 to
 * stop the shell (exit builtin), storing the exit code in *ret. */
static int run_line(char **args, int n, int *ret)
{
	/* ---- builtins ---- */
	if (strcmp(args[0], "exit") == 0) {
		*ret = (n > 1) ? atoi(args[1]) : 0;
		return 0;
	}
	if (strcmp(args[0], "cd") == 0) {
		const char *dir = (n > 1) ? args[1] : "/";
		if (chdir(dir) != 0)
			dprintf(2, "cd: %s: cannot change directory\n", dir);
		return 1;
	}
	if (strcmp(args[0], "pwd") == 0) {
		char buf[256];
		if (getcwd(buf, sizeof(buf))) puts(buf);
		return 1;
	}
	if (strcmp(args[0], "exec") == 0) {
		if (n < 2) return 1;                /* `exec` with no args is a no-op */
		try_execve(&args[1]);               /* replace THIS process; no fork */
		dprintf(2, "sh: exec: %s: not found\n", args[1]);
		*ret = 127;
		return 0;                           /* exec failed -> stop the shell */
	}

	/* ---- external command ---- */
	pid_t pid = fork();
	if (pid < 0) { dprintf(2, "sh: fork failed\n"); return 1; }
	if (pid == 0) {
		try_execve(args);                   /* child: returns only on failure */
		dprintf(2, "sh: %s: not found\n", args[0]);
		_exit(127);
	}
	int status = 0;
	waitpid(pid, &status, 0);
	if (WIFSIGNALED(status))
		dprintf(2, "sh: %s killed by signal %d\n", args[0], WTERMSIG(status));
	return 1;
}

/* Read+execute lines from `fd` until EOF or an exit. Prints a prompt only when
 * interactive. Skips blank lines and `#` comments (incl. the `#!` shebang). */
static int run_fd(int fd)
{
	char line[LINELEN];
	char *args[MAXARGS];
	int ret = 0;

	for (;;) {
		if (g_interactive) write(1, "gv3$ ", 5);
		int len = read_line(fd, line, sizeof(line));
		if (len < 0) { if (g_interactive) write(1, "\n", 1); break; }
		if (len == 0) continue;

		/* skip leading whitespace to test for a comment line */
		char *s = line;
		while (*s == ' ' || *s == '\t') s++;
		if (*s == '\0' || *s == '#') continue;   /* blank or comment (incl. #!) */

		int n = tokenize(line, args, MAXARGS);
		if (n == 0) continue;
		if (!run_line(args, n, &ret)) break;
	}
	return ret;
}

int main(int argc, char **argv, char **envp)
{
	g_envp = envp;

	if (argc > 1) {
		/* script mode: run commands from the file (the shebang handoff path). */
		int fd = open(argv[1], O_RDONLY);
		if (fd < 0) { dprintf(2, "sh: cannot open %s\n", argv[1]); return 127; }
		g_interactive = 0;
		int rc = run_fd(fd);
		close(fd);
		return rc;
	}

	/* interactive mode: read from the console. */
	g_interactive = 1;
	return run_fd(0);
}
