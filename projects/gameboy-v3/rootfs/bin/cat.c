/*
 * cat — copy files (or stdin) to stdout. With no args, reads stdin.
 * Exercises open/read/write/close through gv3libc.
 */
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

static int cat_fd(int fd)
{
	char buf[1024];
	ssize_t n;
	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		ssize_t off = 0;
		while (off < n) {
			ssize_t w = write(1, buf + off, (size_t)(n - off));
			if (w < 0) return -1;
			off += w;
		}
	}
	return (n < 0) ? -1 : 0;
}

int main(int argc, char **argv)
{
	if (argc < 2)
		return cat_fd(0) == 0 ? 0 : 1;

	int rc = 0;
	for (int i = 1; i < argc; i++) {
		int fd = open(argv[i], O_RDONLY);
		if (fd < 0) {
			dprintf(2, "cat: %s: cannot open\n", argv[i]);
			rc = 1;
			continue;
		}
		if (cat_fd(fd) != 0) {
			dprintf(2, "cat: %s: read error\n", argv[i]);
			rc = 1;
		}
		close(fd);
	}
	return rc;
}
