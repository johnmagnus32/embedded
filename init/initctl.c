/* initctl.c — the control client for init (see init.c). Connects to /run/initctl.sock, sends the
 * command line as one text line, prints init's reply. Verbs: poweroff | reboot | halt | status [svc]
 * | start <svc> | stop <svc> | restart <svc> | reload. */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#define INITCTL_SOCK "/run/initctl.sock"

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: %s <poweroff|reboot|halt|status|start|stop|restart|reload> [service]\n", argv[0]);
		return 2;
	}

	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) { perror("initctl: socket"); return 1; }
	struct sockaddr_un sa; memset(&sa, 0, sizeof sa);
	sa.sun_family = AF_UNIX;
	strncpy(sa.sun_path, INITCTL_SOCK, sizeof sa.sun_path - 1);
	if (connect(fd, (struct sockaddr *)&sa, sizeof sa) < 0) {
		fprintf(stderr, "initctl: cannot reach init at %s: %s\n", INITCTL_SOCK, strerror(errno));
		return 1;
	}

	/* Join argv[1..] with spaces into one request line. */
	char cmd[256]; size_t off = 0;
	for (int i = 1; i < argc && off < sizeof cmd - 2; i++)
		off += snprintf(cmd + off, sizeof cmd - off, "%s%s", i > 1 ? " " : "", argv[i]);
	off += snprintf(cmd + off, sizeof cmd - off, "\n");
	if (write(fd, cmd, off) < 0) { perror("initctl: write"); return 1; }

	char buf[1024]; ssize_t n;
	while ((n = read(fd, buf, sizeof buf)) > 0) (void)!write(STDOUT_FILENO, buf, n);
	return 0;
}
