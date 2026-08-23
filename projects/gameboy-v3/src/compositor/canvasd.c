/* compositor/canvasd.c — THE COMPOSITOR (Model B). Owns the screen for the life of
 * the session; every other process is a client that hands it buffers. The gameboy-v3
 * analogue of vi/nvnflinger (Switch) / Mutter / SurfaceFlinger.
 *
 * DISPLAY + INPUT live behind a backend seam (backend.h) — exactly one backend_*.c is
 * linked per build: backend_drm.c (T113: DRM/KMS + evdev) or backend_web.c (host sim:
 * memfd + software compose + a browser over WebSocket). canvasd itself is
 * backend-agnostic: the socket server, client table, focus, and the frame loop.
 * Spawning games is NOT here — that's appletd; the compositor never fork/execs.
 */
#define _GNU_SOURCE
#include "canvas-proto.h"
#include "backend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

#define MAX_CLIENTS 8

struct client {
	int          sock;                     /* -1 = slot free */
	enum canvas_role role;
	uint32_t     nbufs;
	int          submitted;                /* latest SUBMIT'd buffer index, -1 if none */
	int          onscreen;                 /* buffer currently shown, -1 if none */
	int          fd[CANVAS_MAX_BUFFERS];   /* backend fds for this client's buffers (-1 = none) */
};

static struct client clients[MAX_CLIENTS];
static int foreground = -1;                /* GAME-role client with focus */
static int launcher = -1;                  /* focus fallback (the home menu) */
static uint32_t g_w = 800, g_h = 480;      /* set from backend_init */

/* Forward one normalized event to whoever currently has focus. */
static void forward_input(uint32_t button, int32_t value)
{
	int who = foreground >= 0 ? foreground : launcher;
	if (who < 0) return;
	struct canvas_msg m = { .op = CANVAS_INPUT, .u.input = { .button = button, .value = value } };
	canvas_send(clients[who].sock, &m, -1);
}

static int alloc_slot(void)
{
	for (int i = 0; i < MAX_CLIENTS; i++) if (clients[i].sock < 0) return i;
	return -1;
}

static void set_focus(int idx)
{
	if (foreground == idx) return;
	if (foreground >= 0) {
		struct canvas_msg m = { .op = CANVAS_FOCUS, .u.focus.focused = 0 };
		canvas_send(clients[foreground].sock, &m, -1);
	}
	foreground = idx;
	if (idx >= 0) {
		struct canvas_msg m = { .op = CANVAS_FOCUS, .u.focus.focused = 1 };
		canvas_send(clients[idx].sock, &m, -1);
	}
}

static void on_hello(int idx, const struct canvas_msg *hello)
{
	struct client *cl = &clients[idx];
	cl->role = hello->u.hello.role;
	cl->nbufs = CANVAS_MAX_BUFFERS;
	cl->submitted = cl->onscreen = -1;

	struct canvas_msg w = { .op = CANVAS_WELCOME, .u.welcome = {
		.width = g_w, .height = g_h, .format = CANVAS_FMT_XRGB8888, .nbufs = cl->nbufs } };
	canvas_send(cl->sock, &w, -1);

	for (uint32_t i = 0; i < cl->nbufs; i++) {
		uint32_t stride, size;
		int fd = backend_alloc_buffer(&stride, &size);
		struct canvas_msg b = { .op = CANVAS_BUFFER,
			.u.buffer = { .index = i, .stride = stride, .size = size } };
		canvas_send(cl->sock, &b, fd);     /* passes a dup to the client; we KEEP fd for present() */
		cl->fd[i] = fd;                     /* -1 on the drm stub -> client mmap fails and bails */
	}

	if (cl->role == CANVAS_ROLE_GAME) {
		if (launcher < 0) launcher = idx;  /* first GAME client == the launcher */
		set_focus(idx);
	}
}

static void drop_client(int idx)
{
	for (int k = 0; k < CANVAS_MAX_BUFFERS; k++)
		if (clients[idx].fd[k] >= 0) { close(clients[idx].fd[k]); clients[idx].fd[k] = -1; }
	close(clients[idx].sock);
	clients[idx].sock = -1;
	if (foreground == idx) set_focus(launcher);
	if (launcher == idx) launcher = -1;
}

static void on_client_msg(int idx)
{
	struct canvas_msg m;
	if (canvas_recv(clients[idx].sock, &m, NULL) < 0) { drop_client(idx); return; }
	switch (m.op) {
	case CANVAS_HELLO:  on_hello(idx, &m); break;
	case CANVAS_SUBMIT: clients[idx].submitted = (int)m.u.submit.index; break;
	case CANVAS_QUIT:   drop_client(idx); break;
	default: break;
	}
}

/* After presenting: RELEASE each client's old on-screen buffer so it can reuse it. */
static void reap_flip(void)
{
	for (int i = 0; i < MAX_CLIENTS; i++) {
		struct client *cl = &clients[i];
		if (cl->sock < 0 || cl->submitted < 0) continue;
		if (cl->onscreen >= 0 && cl->onscreen != cl->submitted) {
			struct canvas_msg r = { .op = CANVAS_RELEASE, .u.release.index = (uint32_t)cl->onscreen };
			canvas_send(cl->sock, &r, -1);
		}
		cl->onscreen = cl->submitted;
	}
}

static int listen_sock(void)
{
	mkdir("/run/canvas", 0755);            /* best-effort; the sim uses a /tmp CANVAS_SOCK */
	unlink(canvas_sock_path());
	int s = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	struct sockaddr_un sa = { .sun_family = AF_UNIX };
	strncpy(sa.sun_path, canvas_sock_path(), sizeof(sa.sun_path) - 1);
	if (bind(s, (struct sockaddr *)&sa, sizeof(sa)) < 0 || listen(s, 8) < 0) {
		perror("canvasd: listen"); exit(1);
	}
	return s;
}

int main(void)
{
	for (int i = 0; i < MAX_CLIENTS; i++) {
		clients[i].sock = -1;
		for (int k = 0; k < CANVAS_MAX_BUFFERS; k++) clients[i].fd[k] = -1;
	}
	if (backend_init(&g_w, &g_h) < 0) { fprintf(stderr, "canvasd: backend_init failed\n"); return 1; }
	int ls = listen_sock();
	fprintf(stderr, "canvasd: up. %ux%u, socket %s\n", g_w, g_h, canvas_sock_path());

	for (;;) {
		struct pollfd pfds[1 + MAX_CLIENTS];
		int map[1 + MAX_CLIENTS], n = 0;
		pfds[n].fd = ls; pfds[n].events = POLLIN; map[n] = -1; n++;
		for (int i = 0; i < MAX_CLIENTS; i++)
			if (clients[i].sock >= 0) {
				pfds[n].fd = clients[i].sock; pfds[n].events = POLLIN; map[n] = i; n++;
			}

		if (poll(pfds, n, 16) < 0 && errno != EINTR) break;   /* ~60Hz tick */

		if (pfds[0].revents & POLLIN) {
			int c = accept(ls, NULL, NULL);
			int slot = c >= 0 ? alloc_slot() : -1;
			if (c >= 0 && slot < 0) close(c);
			else if (c >= 0) {
				clients[slot].sock = c;
				for (int k = 0; k < CANVAS_MAX_BUFFERS; k++) clients[slot].fd[k] = -1;
			}
		}
		for (int k = 1; k < n; k++)
			if (pfds[k].revents & POLLIN) on_client_msg(map[k]);

		int fg = -1;
		if (foreground >= 0 && clients[foreground].sock >= 0 && clients[foreground].submitted >= 0)
			fg = clients[foreground].fd[clients[foreground].submitted];
		backend_present(fg);
		reap_flip();

		uint32_t btn; int32_t val;
		while (backend_poll_input(&btn, &val)) forward_input(btn, val);
	}
	backend_fini();
	return 0;
}
