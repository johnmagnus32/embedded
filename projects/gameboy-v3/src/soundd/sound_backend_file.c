/* sound_backend_file.c — the host-test output backend: append the mixed PCM to a raw int16-stereo
 * file ($SOUND_FILE, else /tmp/soundd_out.raw). Pacing is a timerfd (one tick per period) that
 * soundd polls — so this needs no sleep, and the whole pipeline behaves like the device would.
 * Lets the ring/IPC/mix path be verified without a browser or audio hardware. */
#include "sound_backend.h"
#include "sound-proto.h"
#include <stdio.h>
#include <stdlib.h>

#define PERIOD 1024                       /* frames per write (~23ms @ 44100) */
static FILE *g_f;
static int   g_timer = -1;

int abk_init(uint32_t *rate, uint32_t *channels, uint32_t *period)
{
	const char *p = getenv("SOUND_FILE"); if (!p || !*p) p = "/tmp/soundd_out.raw";
	g_f = fopen(p, "wb");
	if (!g_f) { perror("soundd(file): open"); return -1; }
	*rate = SOUND_RATE; *channels = SOUND_CH; *period = PERIOD;
	g_timer = abk_timerfd(PERIOD, SOUND_RATE);
	if (g_timer < 0) { fclose(g_f); g_f = NULL; return -1; }
	fprintf(stderr, "soundd: file backend -> %s (%d Hz, %d ch, period %d)\n", p, SOUND_RATE, SOUND_CH, PERIOD);
	return 0;
}

int abk_pollfds(struct pollfd *pfds, int max)
{
	if (max < 1) return 0;
	pfds[0].fd = g_timer; pfds[0].events = POLLIN;
	return 1;
}

int abk_ready(struct pollfd *pfds, int n)
{
	for (int i = 0; i < n; i++)
		if (pfds[i].fd == g_timer && (pfds[i].revents & POLLIN)) return abk_timer_ticks(g_timer);
	return 0;
}

void abk_write(const int16_t *pcm, uint32_t frames)
{
	if (g_f) fwrite(pcm, sizeof(int16_t) * SOUND_CH, frames, g_f);   /* buffered; no pacing here */
}

void abk_fini(void)
{
	if (g_f) fclose(g_f);
	if (g_timer >= 0) close(g_timer);
}
