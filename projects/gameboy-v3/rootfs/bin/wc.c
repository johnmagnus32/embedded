/*
 * wc — count lines, words, and bytes of files (or stdin). Proves the
 * auto-discovery refactor: dropping this file into bin/ ships it, no Makefile edit.
 */
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

static void count_fd(int fd, const char *name)
{
	long lines = 0, words = 0, bytes = 0;
	int in_word = 0;
	char buf[1024];
	ssize_t n;
	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		for (ssize_t i = 0; i < n; i++) {
			bytes++;
			char c = buf[i];
			if (c == '\n') lines++;
			if (c == ' ' || c == '\t' || c == '\n') in_word = 0;
			else if (!in_word) { in_word = 1; words++; }
		}
	}
	dprintf(1, "%d %d %d %s\n", (int)lines, (int)words, (int)bytes, name ? name : "");
}

int main(int argc, char **argv)
{
	if (argc < 2) { count_fd(0, ""); return 0; }
	for (int i = 1; i < argc; i++) {
		int fd = open(argv[i], O_RDONLY);
		if (fd < 0) { dprintf(2, "wc: %s: cannot open\n", argv[i]); continue; }
		count_fd(fd, argv[i]);
		close(fd);
	}
	return 0;
}
