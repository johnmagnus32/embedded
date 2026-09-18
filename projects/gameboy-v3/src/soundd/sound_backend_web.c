/* sound_backend_web.c — soundd's host-sim output: a tiny WebSocket server on its OWN port
 * (SOUND_WEB_PORT, default 8081) that streams raw int16 stereo PCM to the browser, played there via
 * WebAudio. The audio twin of the compositor's backend_web.c video WS — a SEPARATE socket/port so a
 * big (~79 KB) video frame can't head-of-line-block small audio chunks. PCM is sent RAW (deflate
 * barely helps on PCM and adds latency).
 *
 * Pattern A: this backend's poll fds (a period timerfd + the WS listener + each browser conn) go into
 * soundd's single poll(); abk_ready() accepts new browsers, reaps closed ones, and reports how many
 * periods the timer fired; abk_write() only sends (non-blocking, no pacing). Reuses
 * compositor/websocket.c (ws_handshake / ws_send_binary / ws_recv) — hence -Icompositor in the build.
 */
#include "sound_backend.h"
#include "sound-proto.h"
#include "websocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAXWS  4
#define PERIOD 1024
static int listen_fd = -1;
static int g_timer   = -1;
static int ws[MAXWS];

int abk_init(uint32_t *rate, uint32_t *channels, uint32_t *period)
{
	signal(SIGPIPE, SIG_IGN);                 /* a browser that closed mid-send must not kill soundd */
	for (int i = 0; i < MAXWS; i++) ws[i] = -1;

	const char *ps = getenv("SOUND_WEB_PORT");
	int port = (ps && *ps) ? atoi(ps) : 8081;
	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0) { perror("soundd(web): socket"); return -1; }
	int one = 1; setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
	struct sockaddr_in sa = { .sin_family = AF_INET, .sin_port = htons((uint16_t)port),
	                          .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
	if (bind(listen_fd, (struct sockaddr *)&sa, sizeof sa) < 0 || listen(listen_fd, 8) < 0) {
		perror("soundd(web): bind/listen"); close(listen_fd); listen_fd = -1; return -1;
	}
	*rate = SOUND_RATE; *channels = SOUND_CH; *period = PERIOD;
	g_timer = abk_timerfd(PERIOD, SOUND_RATE);
	if (g_timer < 0) { close(listen_fd); listen_fd = -1; return -1; }
	fprintf(stderr, "soundd: web backend — PCM on ws://localhost:%d  (%d Hz, %d ch)\n",
	        port, SOUND_RATE, SOUND_CH);
	return 0;
}

/* Accept one pending browser (called only when the listener polled readable) + complete the WS
 * handshake; keep the fd. */
static void accept_ws(void)
{
	int c = accept(listen_fd, NULL, NULL);
	if (c < 0) return;
	char req[1024];
	int r = (int)recv(c, req, sizeof req - 1, 0);
	if (r <= 0) { close(c); return; }
	req[r] = 0;
	if (!ws_handshake(c, req)) { close(c); return; }   /* not a WebSocket upgrade */
	for (int i = 0; i < MAXWS; i++) if (ws[i] < 0) { ws[i] = c; return; }
	close(c);                                          /* pool full */
}

int abk_pollfds(struct pollfd *pfds, int max)
{
	int k = 0;
	if (k < max) { pfds[k].fd = g_timer;   pfds[k].events = POLLIN; k++; }
	if (k < max) { pfds[k].fd = listen_fd; pfds[k].events = POLLIN; k++; }
	for (int i = 0; i < MAXWS; i++)
		if (ws[i] >= 0 && k < max) { pfds[k].fd = ws[i]; pfds[k].events = POLLIN; k++; }
	return k;
}

int abk_ready(struct pollfd *pfds, int n)
{
	int due = 0;
	for (int i = 0; i < n; i++) {
		if (!(pfds[i].revents & POLLIN)) continue;
		int fd = pfds[i].fd;
		if (fd == g_timer)        due = abk_timer_ticks(g_timer);
		else if (fd == listen_fd) accept_ws();
		else {                                          /* a browser conn: EOF/ctrl -> reap */
			for (int j = 0; j < MAXWS; j++)
				if (ws[j] == fd) {
					char b[128];
					if (ws_recv(fd, b, sizeof b) < 0) { close(fd); ws[j] = -1; }
					break;
				}
		}
	}
	return due;
}

void abk_write(const int16_t *pcm, uint32_t frames)
{
	size_t bytes = (size_t)frames * SOUND_CH * sizeof(int16_t);
	for (int i = 0; i < MAXWS; i++)
		if (ws[i] >= 0 && ws_send_binary(ws[i], pcm, bytes) < 0) { close(ws[i]); ws[i] = -1; }
}

void abk_fini(void)
{
	for (int i = 0; i < MAXWS; i++) if (ws[i] >= 0) close(ws[i]);
	if (listen_fd >= 0) close(listen_fd);
	if (g_timer >= 0) close(g_timer);
}
