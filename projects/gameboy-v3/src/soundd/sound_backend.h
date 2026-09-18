/* sound_backend.h — soundd's output-device seam (the audio twin of backend.h). soundd is
 * backend-agnostic: it mixes all clients' rings and calls these by name. Exactly ONE
 * sound_backend_*.c is linked per build (Makefile SOUND_BACKEND):
 *   file  -> sound_backend_file.c : write mixed PCM to a raw file (host tests)
 *   web   -> sound_backend_web.c  : stream PCM to the browser over WebSocket -> WebAudio (the sim)
 *   alsa  -> sound_backend_alsa.c : the T113 codec via ALSA (real hw; stubbed for now)
 *
 * PACING = pattern A: the backend exposes a POLLABLE clock fd (abk_pollfds), soundd blocks in one
 * poll() over {that fd + client sockets}, and mixes a period only when the clock fires (abk_ready).
 * So abk_write is NON-blocking — it just hands off one period; the clock, not a sleep, paces the loop.
 * For a real device the clock is the PCM fd (POLLOUT when it wants more); the file/web/alsa-stub
 * backends have no device fd, so they use a timerfd (abk_timerfd) ticking once per period.
 */
#ifndef SOUND_BACKEND_H
#define SOUND_BACKEND_H
#include <stdint.h>
#include <poll.h>
#include <unistd.h>
#include <time.h>
#include <sys/timerfd.h>

#define ABK_MAXFDS 8      /* max poll fds a backend contributes (web: timer + listener + clients) */

int  abk_init(uint32_t *rate, uint32_t *channels, uint32_t *period);  /* set up output; report format */
int  abk_pollfds(struct pollfd *pfds, int max);   /* fill the backend's poll fds; return the count   */
int  abk_ready(struct pollfd *pfds, int n);        /* after poll: service backend fds; return # of      *
                                                    * periods due (0 = clock hasn't fired this wake)    */
void abk_write(const int16_t *pcm, uint32_t frames);   /* output one period (NON-blocking; no pacing)   */
void abk_fini(void);

/* Shared timerfd clock for backends without a native pollable device (file/web/alsa-stub). */
static inline int abk_timerfd(uint32_t period, uint32_t rate)
{
	int fd = timerfd_create(CLOCK_MONOTONIC, 0);
	if (fd < 0) return -1;
	long ns = (long)(1e9 * (double)period / (double)rate);   /* one period, in ns */
	struct itimerspec it = { { ns / 1000000000L, ns % 1000000000L },
	                         { ns / 1000000000L, ns % 1000000000L } };
	timerfd_settime(fd, 0, &it, NULL);
	return fd;
}

/* Consume the timerfd and return how many periods elapsed since last call (0 if it didn't fire);
 * capped so a long stall can't make soundd write a huge burst. */
static inline int abk_timer_ticks(int fd)
{
	uint64_t exp = 0;
	if (read(fd, &exp, sizeof exp) != (ssize_t)sizeof exp) return 0;
	return (int)(exp > 8 ? 8 : exp);
}

#endif /* SOUND_BACKEND_H */
