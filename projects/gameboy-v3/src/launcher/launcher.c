/* launcher/launcher.c — the home menu. A CANVAS_ROLE_GAME client that draws a list
 * of games and, on A, asks *appletd* (the application/applet manager) to run one.
 * It does NOT spawn games itself and does NOT go through the compositor for launch —
 * launching is a process-lifecycle action, which is appletd's job. When the game
 * exits, the compositor refocuses this menu (its socket never closed — it stays
 * resident) and it repaints.
 *
 * MOCK: the game list is static. A real build reads it from libraryd's manifest —
 * an SD scan of /data/apps for native game binaries (each is a canvas client we
 * built, like canvas-breakout). Drawing is flat-color tiles (no GPU).
 */
#include "canvas.h"
#include "appletd-proto.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

struct entry { const char *name; const char *exec; uint32_t color; };
static struct entry games[] = {
	{ "Breakout",   "/usr/bin/canvas-breakout",   0x00e05050 },
	{ "Shmup",      "/usr/bin/canvas-shmup",      0x005050e0 },
	{ "Platformer", "/usr/bin/canvas-platformer", 0x0050c060 },
	{ "Adventure",  "/usr/bin/canvas-adventure",  0x00c0a040 },
};
#define NGAMES ((int)(sizeof(games) / sizeof(games[0])))

/* Ask appletd to run `path` as the Application. One short connection per launch
 * (mock): connect, send LAUNCH_APP, read the ack, close. appletd terminates any
 * current game, fork/execs this one, and it then connects to the compositor itself. */
static void launch_app(const char *path)
{
	int s = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (s < 0) return;
	struct sockaddr_un sa = { .sun_family = AF_UNIX };
	strncpy(sa.sun_path, APPLETD_SOCK_PATH, sizeof(sa.sun_path) - 1);
	if (connect(s, (struct sockaddr *)&sa, sizeof(sa)) == 0) {
		struct appletd_msg m = { .op = APPLETD_LAUNCH_APP };
		strncpy(m.u.launch.path, path, sizeof(m.u.launch.path) - 1);
		appletd_send(s, &m);
		appletd_recv(s, &m);   /* ack (APPLETD_OK/ERR); ignored in the mock */
	}
	close(s);
}

static void fill(canvas_frame *f, int x, int y, int w, int h, uint32_t c)
{
	for (int j = y; j < y + h; j++) {
		if (j < 0 || j >= (int)f->height) continue;
		uint32_t *row = (uint32_t *)((char *)f->pixels + j * f->stride);
		for (int i = x; i < x + w; i++)
			if (i >= 0 && i < (int)f->width) row[i] = c;
	}
}

static void draw(canvas_frame *f, int sel)
{
	fill(f, 0, 0, f->width, f->height, 0x00101014);      /* background */
	int pad = 24, h = 70, y = pad;
	for (int i = 0; i < NGAMES; i++, y += h + pad) {
		if (i == sel)                                /* white selection border */
			fill(f, pad - 4, y - 4, f->width - 2 * pad + 8, h + 8, 0x00ffffff);
		fill(f, pad, y, f->width - 2 * pad, h, games[i].color);
	}
}

int main(void)
{
	canvas_ctx *c = canvas_connect(CANVAS_ROLE_GAME);
	if (!c) { fprintf(stderr, "launcher: no compositor\n"); return 1; }

	int sel = 0;
	for (;;) {
		canvas_input_event ev;
		while (canvas_poll_input(c, &ev)) {
			if (ev.value != 1) continue;         /* act on press */
			switch (ev.button) {
			case CANVAS_BTN_DOWN: sel = (sel + 1) % NGAMES; break;
			case CANVAS_BTN_UP:   sel = (sel + NGAMES - 1) % NGAMES; break;
			case CANVAS_BTN_A:    launch_app(games[sel].exec); break;
			default: break;
			}
		}
		/* Redraw every tick — cheap, and it repaints for free when the
		 * compositor hands focus back after a launched game exits. */
		canvas_frame *f = canvas_acquire(c);
		if (f) { draw(f, sel); canvas_submit(c, f); }
		usleep(16000);
	}
	canvas_disconnect(c);
	return 0;
}
