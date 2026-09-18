/* soundd.c — the gameboy-v3 audio server (the audio twin of canvasd). A single-threaded SEQPACKET
 * socket server that hands each client ONE shared-memory PCM ring (memfd via SCM_RIGHTS) and, each
 * audio period, MIXES every client's ring into the output backend. Structurally a clone of canvasd
 * with two deliberate differences (see the soundd map): (1) one PCM ring per client instead of N
 * swap buffers, with monotonic written/consumed cursors; (2) it mixes ALL clients unconditionally
 * (no focus gate — that inversion vs the compositor is the whole point of an audio server). PCM
 * lives only in the shared rings; the socket carries just control + cursors.
 */
#include "sound-proto.h"
#include "sound_backend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/syscall.h>

#ifndef SYS_memfd_create          /* glibc < 2.27 (Amazon Linux 2) lacks the wrapper */
#define SYS_memfd_create __NR_memfd_create
#endif

#define MAX_CLIENTS 8
#define RING_PERIODS 3            /* ring capacity in output periods (lower = less SFX latency, less underrun headroom) */
#define MAX_PERIOD  4096

struct client {
	int      sock;                /* -1 = slot free */
	int      ring_fd;             /* kept memfd (soundd re-reads the shared PCM to mix) */
	int16_t *ring;                /* soundd's own mmap of that ring */
	uint32_t ring_frames;
	size_t   ring_bytes;
	uint64_t written, consumed;   /* monotonic frame cursors (client-written / soundd-drained) */
};
static struct client clients[MAX_CLIENTS];
static uint32_t g_rate, g_ch, g_period;

static int alloc_slot(void)
{
	for (int i = 0; i < MAX_CLIENTS; i++) if (clients[i].sock < 0) return i;
	return -1;
}

static void drop_client(int idx)
{
	struct client *cl = &clients[idx];
	if (cl->ring) { munmap(cl->ring, cl->ring_bytes); cl->ring = NULL; }
	if (cl->ring_fd >= 0) { close(cl->ring_fd); cl->ring_fd = -1; }
	close(cl->sock); cl->sock = -1;
}

/* Allocate a shared PCM ring memfd (the memfd/ftruncate mechanic copied from backend_web). */
static int alloc_ring(uint32_t frames, size_t *bytes)
{
	*bytes = (size_t)frames * g_ch * sizeof(int16_t);
	int fd = (int)syscall(SYS_memfd_create, "sndring", 0u);
	if (fd < 0) return -1;
	if (ftruncate(fd, (off_t)*bytes) < 0) { close(fd); return -1; }
	return fd;
}

static void on_hello(int idx, const struct sound_msg *hello)
{
	struct client *cl = &clients[idx];
	(void)hello;                                   /* role unused for now */
	cl->ring_frames = g_period * RING_PERIODS;
	cl->written = cl->consumed = 0;

	size_t bytes; int fd = alloc_ring(cl->ring_frames, &bytes);
	if (fd < 0) { drop_client(idx); return; }
	void *p = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (p == MAP_FAILED) { close(fd); drop_client(idx); return; }
	cl->ring = p; cl->ring_fd = fd; cl->ring_bytes = bytes;

	struct sound_msg w = { .op = SOUND_WELCOME, .u.welcome = {
		.rate = g_rate, .channels = g_ch, .ring_frames = cl->ring_frames, .ring_bytes = (uint32_t)bytes } };
	sound_send(cl->sock, &w, -1);
	struct sound_msg r = { .op = SOUND_RING, .u.ring.size = (uint32_t)bytes };
	sound_send(cl->sock, &r, fd);                  /* passes a dup; we KEEP fd for mixing */
}

static void on_client_msg(int idx)
{
	struct sound_msg m;
	if (sound_recv(clients[idx].sock, &m, NULL) < 0) { drop_client(idx); return; }
	switch (m.op) {
	case SOUND_HELLO:  on_hello(idx, &m); break;
	case SOUND_SUBMIT: clients[idx].written = m.u.submit.written; break;
	case SOUND_QUIT:   drop_client(idx); break;
	default: break;
	}
}

/* Mix every client's available frames (up to one period) into the output, then tell each client
 * how far we've drained so it can refill. Missing frames (a slow client) mix as silence. */
static void mix_and_output(void)
{
	static int32_t acc[MAX_PERIOD * SOUND_CH];
	static int16_t out[MAX_PERIOD * SOUND_CH];
	uint32_t period = g_period, ch = g_ch;
	memset(acc, 0, sizeof(int32_t) * period * ch);

	for (int i = 0; i < MAX_CLIENTS; i++) {
		struct client *cl = &clients[i];
		if (cl->sock < 0 || !cl->ring) continue;
		uint64_t avail = cl->written - cl->consumed;
		uint32_t n = avail > period ? period : (uint32_t)avail;
		uint32_t rpos = (uint32_t)(cl->consumed % cl->ring_frames);
		for (uint32_t f = 0; f < n; f++) {
			uint32_t idx = ((rpos + f) % cl->ring_frames) * ch;
			for (uint32_t c = 0; c < ch; c++) acc[f * ch + c] += cl->ring[idx + c];
		}
		if (n) {
			cl->consumed += n;
			struct sound_msg m = { .op = SOUND_CONSUMED, .u.consumed.consumed = cl->consumed };
			sound_send(cl->sock, &m, -1);
		}
	}

	for (uint32_t j = 0; j < period * ch; j++) {
		int32_t v = acc[j];
		out[j] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);   /* sum + clamp */
	}
	abk_write(out, period);                        /* non-blocking hand-off; the poll clock paces */
}

static int listen_sock(void)
{
	const char *path = sound_sock_path();
	int s = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (s < 0) { perror("soundd: socket"); return -1; }
	mkdir("/run/sound", 0755);            /* best-effort; the sim uses a /tmp SOUND_SOCK */
	struct sockaddr_un sa = { .sun_family = AF_UNIX };
	strncpy(sa.sun_path, path, sizeof sa.sun_path - 1);
	unlink(path);
	if (bind(s, (struct sockaddr *)&sa, sizeof sa) < 0) { perror("soundd: bind"); close(s); return -1; }
	if (listen(s, MAX_CLIENTS) < 0) { perror("soundd: listen"); close(s); return -1; }
	return s;
}

int main(void)
{
	for (int i = 0; i < MAX_CLIENTS; i++) { clients[i].sock = -1; clients[i].ring_fd = -1; }
	if (abk_init(&g_rate, &g_ch, &g_period) < 0) { fprintf(stderr, "soundd: backend init failed\n"); return 1; }
	if (g_period > MAX_PERIOD) g_period = MAX_PERIOD;
	int ls = listen_sock();
	if (ls < 0) return 1;

	for (;;) {
		/* Pattern A: one BLOCKING poll over the client sockets AND the backend's clock fd. We mix a
		 * period only when the clock fires (abk_ready), so the audio device — not a sleep — paces the
		 * loop, and control messages are serviced in the same wait instead of stalling behind a write. */
		struct pollfd pfds[1 + MAX_CLIENTS + ABK_MAXFDS]; int map[1 + MAX_CLIENTS]; int n = 0;
		pfds[n].fd = ls; pfds[n].events = POLLIN; map[n] = -1; n++;
		for (int i = 0; i < MAX_CLIENTS; i++)
			if (clients[i].sock >= 0) { pfds[n].fd = clients[i].sock; pfds[n].events = POLLIN; map[n] = i; n++; }
		int nsock = n;                                    /* sockets occupy [0, nsock) */
		n += abk_pollfds(&pfds[n], ABK_MAXFDS);           /* backend clock/conn fds follow */

		if (poll(pfds, n, -1) > 0) {
			if (pfds[0].revents & POLLIN) {              /* new client */
				int c = accept(ls, NULL, NULL);
				if (c >= 0) { int idx = alloc_slot(); if (idx < 0) close(c); else clients[idx].sock = c; }
			}
			for (int k = 1; k < nsock; k++)
				if (pfds[k].revents & POLLIN) on_client_msg(map[k]);
			int due = abk_ready(pfds, n);                 /* periods the clock fired this wake (0..N) */
			while (due-- > 0) mix_and_output();           /* mix+output one period each; catches up after a stall */
		}
	}
}
