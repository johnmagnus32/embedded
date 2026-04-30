/*
 * max98357a.c — MAX98357A I2S DAC emulation (wall-clock paced)
 *
 * Pulls samples from guest RAM via DMA source address on a wall-clock
 * timer. Fires DMA half-transfer and transfer-complete interrupts at
 * the correct rate so the firmware audio task wakes up realistically.
 */
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "max98357a.h"
#include "stm32_dma.h"
#include "membus.h"

void max98357a_init(struct max98357a *d)
{
    memset(d, 0, sizeof(*d));
    d->sample_rate = 22050;
}

void max98357a_tick(struct max98357a *d)
{
    struct stm32_dma_stream *s = d->dma_stream;
    if (!s || !(s->cr & (1 << 0)) || !d->bus) /* not enabled */
        return;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    if (!d->started) {
        d->last_pull = now;
        d->started = 1;
        return;
    }

    double elapsed = (now.tv_sec - d->last_pull.tv_sec)
                   + (now.tv_nsec - d->last_pull.tv_nsec) / 1e9;

    /* How many 16-bit words should have been transferred? */
    int words_due = (int)(elapsed * d->sample_rate * 2); /* L+R per sample */
    if (words_due <= 0)
        return;

    /* Cap to what's available */
    if (words_due > (int)s->ndtr)
        words_due = (int)s->ndtr;

    /* Pull samples from guest RAM */
    int16_t buf[1024];
    int total = 0;
    while (total < words_due) {
        int chunk = words_due - total;
        if (chunk > 1024) chunk = 1024;

        for (int i = 0; i < chunk; i++) {
            buf[i] = (int16_t)membus_read16(d->bus, s->m0ar);
            if (s->cr & (1 << 10)) /* MINC */
                s->m0ar += 2;
            s->ndtr--;

            /* Half-transfer interrupt */
            if (s->ndtr == s->ndtr_orig / 2)
                stm32_dma_fire_htif(d->dma, d->dma_stream_idx);

            /* Transfer complete */
            if (s->ndtr == 0) {
                stm32_dma_fire_tcif(d->dma, d->dma_stream_idx);
                if (!(s->cr & (1 << 0))) /* stream disabled by tcif (non-circular) */
                    goto done;
            }
        }

        /* Send to chardev (non-blocking — drop audio rather than stall) */
        if (d->cd && d->cd->client_fd >= 0) {
            int flags = fcntl(d->cd->client_fd, F_GETFL, 0);
            fcntl(d->cd->client_fd, F_SETFL, flags | O_NONBLOCK);
            write(d->cd->client_fd, buf, chunk * 2);
            fcntl(d->cd->client_fd, F_SETFL, flags);
        }

        total += chunk;
    }
done:
    d->last_pull = now;
}
