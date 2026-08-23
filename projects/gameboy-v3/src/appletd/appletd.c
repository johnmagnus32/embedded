/* appletd/appletd.c — the application/applet manager (the Switch am+pm analog).
 * Owns the app-layer PROCESS TREE: at boot it spawns the resident home menu, then
 * launches games on request (APPLETD_LAUNCH_APP), enforces one Application at a
 * time, and reaps children. It does NOT draw — the home menu and games are display
 * clients of the compositor (canvasd); appletd is their PARENT. The compositor
 * never spawns anything (that split mirrors the Switch: vi vs am/pm).
 *
 * PHASE 1 (this file): launch + one-at-a-time + reap. Focus still rides canvasd's
 * client connect/disconnect — when a game's compositor socket closes, canvasd
 * refocuses the (still-resident) home menu, so appletd only needs to reap here.
 * PHASE 2 (TODO): a control link to canvasd for HOME/suspend/resume + the resident
 * home overlay; a launch token threaded appletd -> child -> canvasd; memory pools.
 */
#define _GNU_SOURCE
#include "appletd-proto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define HOME_MENU "/usr/bin/canvas-launcher"

static pid_t home_pid = -1;   /* the resident home menu (respawned if it dies) */
static pid_t app_pid  = -1;   /* the one running Application, -1 if none */

static pid_t spawn(const char *path)
{
	pid_t pid = fork();
	if (pid == 0) {
		/* TODO(phase2): set CANVAS_TOKEN env + a memory cgroup before exec. */
		execl(path, path, (char *)NULL);
		_exit(127);
	}
	return pid;   /* -1 on fork failure */
}

/* One Application at a time: stop the current game before starting another. */
static void terminate_app(void)
{
	if (app_pid <= 0) return;
	kill(app_pid, SIGTERM);
	/* TODO(phase2): grace period then SIGKILL. The reap arrives via the loop. */
}

static void on_launch(const struct appletd_msg *m, int reply)
{
	terminate_app();                     /* unload the previous game */
	app_pid = spawn(m->u.launch.path);
	struct appletd_msg r = { .op = app_pid > 0 ? APPLETD_OK : APPLETD_ERR };
	appletd_send(reply, &r);
}

/* Reap dead children: clear app_pid when the game exits (canvasd already refocused
 * the home menu via the closed socket); respawn the home menu if IT died — it's
 * meant to be resident, like the Switch's qlaunch. */
static void reap_children(void)
{
	int status;
	pid_t pid;
	while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		if (pid == app_pid) {
			app_pid = -1;
		} else if (pid == home_pid) {
			fprintf(stderr, "appletd: home menu exited — respawning\n");
			home_pid = spawn(HOME_MENU);
		}
	}
}

static int listen_sock(void)
{
	mkdir("/run/canvas", 0755);
	unlink(APPLETD_SOCK_PATH);
	int s = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	struct sockaddr_un sa = { .sun_family = AF_UNIX };
	strncpy(sa.sun_path, APPLETD_SOCK_PATH, sizeof(sa.sun_path) - 1);
	if (bind(s, (struct sockaddr *)&sa, sizeof(sa)) < 0 || listen(s, 8) < 0) {
		perror("appletd: listen"); exit(1);
	}
	return s;
}

int main(void)
{
	int ls = listen_sock();
	home_pid = spawn(HOME_MENU);   /* the resident home menu (qlaunch analog) */
	fprintf(stderr, "appletd: up. home menu pid %d, socket %s\n",
		(int)home_pid, APPLETD_SOCK_PATH);

	for (;;) {
		struct pollfd pfd = { .fd = ls, .events = POLLIN };
		if (poll(&pfd, 1, 200) < 0 && errno != EINTR) break;
		reap_children();                       /* at least every ~200 ms */
		if (pfd.revents & POLLIN) {
			int c = accept(ls, NULL, NULL);
			if (c < 0) continue;
			struct appletd_msg m;
			if (appletd_recv(c, &m) == 0 && m.op == APPLETD_LAUNCH_APP)
				on_launch(&m, c);
			close(c);   /* one request per connection (the launcher reconnects) */
		}
	}
	return 0;
}
