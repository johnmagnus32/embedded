/* libsound/sound.c — the audio client SDK, the twin of libcanvas/canvas.c. Mirrors canvas's
 * connect/handshake/SCM_RIGHTS-fd/mmap mechanics, but the payload is ONE contiguous PCM ring with
 * a monotonic write cursor (not N discrete swap buffers). */
#include "sound.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>

struct sound_ctx {
	int      sock;
	uint32_t rate, channels, ring_frames, ring_bytes;
	int16_t *ring;                 /* mmap'd shared PCM ring, ring_frames*channels samples */
	uint64_t written, consumed;    /* monotonic frame cursors */
};

sound_ctx *sound_connect(enum sound_role role)
{
	int s = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (s < 0) return NULL;
	struct sockaddr_un sa = { .sun_family = AF_UNIX };
	strncpy(sa.sun_path, sound_sock_path(), sizeof sa.sun_path - 1);
	if (connect(s, (struct sockaddr *)&sa, sizeof sa) < 0) { close(s); return NULL; }

	struct sound_msg m = { .op = SOUND_HELLO, .u.hello.role = (uint32_t)role };
	if (sound_send(s, &m, -1) < 0) { close(s); return NULL; }

	if (sound_recv(s, &m, NULL) < 0 || m.op != SOUND_WELCOME) { close(s); return NULL; }
	uint32_t rate = m.u.welcome.rate, ch = m.u.welcome.channels;
	uint32_t rframes = m.u.welcome.ring_frames, rbytes = m.u.welcome.ring_bytes;

	int fd = -1;
	if (sound_recv(s, &m, &fd) < 0 || m.op != SOUND_RING || fd < 0) { close(s); return NULL; }
	void *p = mmap(NULL, rbytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);                                  /* mapping keeps the memory; fd is disposable */
	if (p == MAP_FAILED) { close(s); return NULL; }

	sound_ctx *c = calloc(1, sizeof *c);
	if (!c) { munmap(p, rbytes); close(s); return NULL; }
	c->sock = s; c->rate = rate; c->channels = ch;
	c->ring_frames = rframes; c->ring_bytes = rbytes; c->ring = p;
	return c;
}

void sound_disconnect(sound_ctx *c)
{
	if (!c) return;
	struct sound_msg m = { .op = SOUND_QUIT };
	sound_send(c->sock, &m, -1);
	munmap(c->ring, c->ring_bytes);
	close(c->sock);
	free(c);
}

uint32_t sound_rate(const sound_ctx *c)     { return c ? c->rate : 0; }
uint32_t sound_channels(const sound_ctx *c) { return c ? c->channels : 0; }

void sound_poll(sound_ctx *c)
{
	if (!c) return;
	struct pollfd pfd = { .fd = c->sock, .events = POLLIN };
	while (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) {
		struct sound_msg m;
		if (sound_recv(c->sock, &m, NULL) < 0) return;
		if (m.op == SOUND_CONSUMED) c->consumed = m.u.consumed.consumed;
	}
}

int sound_avail(sound_ctx *c)
{
	if (!c) return 0;
	uint64_t used = c->written - c->consumed;      /* frames soundd hasn't drained yet */
	if (used > c->ring_frames) used = c->ring_frames;
	return (int)(c->ring_frames - used);
}

int sound_write(sound_ctx *c, const int16_t *pcm, int frames)
{
	if (!c || frames <= 0) return 0;
	int free_frames = sound_avail(c);
	if (frames > free_frames) frames = free_frames;
	if (frames <= 0) return 0;

	uint32_t ch = c->channels;
	uint32_t wpos = (uint32_t)(c->written % c->ring_frames);        /* frame index into the ring */
	uint32_t first = c->ring_frames - wpos;                         /* frames until the ring end */
	if ((uint32_t)frames < first) first = (uint32_t)frames;
	memcpy(c->ring + (size_t)wpos * ch, pcm, (size_t)first * ch * sizeof(int16_t));
	uint32_t rest = (uint32_t)frames - first;
	if (rest) memcpy(c->ring, pcm + (size_t)first * ch, (size_t)rest * ch * sizeof(int16_t));

	c->written += (uint32_t)frames;
	struct sound_msg m = { .op = SOUND_SUBMIT, .u.submit.written = c->written };
	sound_send(c->sock, &m, -1);
	return frames;
}
