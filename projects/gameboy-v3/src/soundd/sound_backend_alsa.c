/* sound_backend_alsa.c — the T113 output backend (the audio twin of backend_drm.c). STILL STUBBED:
 * a real impl opens the Allwinner codec's ALSA PCM (snd_pcm_open, S16_LE, 44100, stereo), primes
 * the buffer, and drives pacing from the PCM's OWN poll fd (snd_pcm_poll_descriptors -> abk_pollfds;
 * snd_pcm_poll_descriptors_revents + POLLOUT -> abk_ready) with a non-blocking snd_pcm_writei +
 * xrun recovery in abk_write. That needs the sun8i-codec ASoC driver + a device-tree audio node +
 * -lasound. Until then this is a silent sink that uses the shared timerfd as its clock (so the
 * pattern-A poll loop behaves), exactly like backend_drm hands out no buffers. */
#include "sound_backend.h"
#include "sound-proto.h"

#define PERIOD 1024
static int g_timer = -1;

int abk_init(uint32_t *rate, uint32_t *channels, uint32_t *period)
{
	*rate = SOUND_RATE; *channels = SOUND_CH; *period = PERIOD;
	g_timer = abk_timerfd(PERIOD, SOUND_RATE);   /* TODO: replace with snd_pcm_poll_descriptors */
	return g_timer < 0 ? -1 : 0;
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
	(void)pcm; (void)frames;   /* TODO: snd_pcm_writei(pcm, frames) + snd_pcm_recover on xrun */
}

void abk_fini(void)
{
	if (g_timer >= 0) close(g_timer);
}
