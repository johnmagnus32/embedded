/*
 * max98357a.c — MAX98357A I2S DAC emulation (push model)
 *
 * Receives stereo samples via the I2S sink interface, buffers them,
 * and flushes to the chardev periodically.
 */
#include <string.h>
#include "max98357a.h"

void max98357a_init(struct max98357a *d)
{
    memset(d, 0, sizeof(*d));
    d->sink.write = max98357a_write;
    d->sink.opaque = d;
}

void max98357a_write(void *opaque, int16_t left, int16_t right)
{
    struct max98357a *d = (struct max98357a *)opaque;

    if (d->buf_len + 2 <= 1024) {
        d->buf[d->buf_len++] = left;
        d->buf[d->buf_len++] = right;
    }

    /* Flush when buffer is full enough */
    if (d->buf_len >= 512 && d->cd) {
        chardev_write_buf(d->cd, (const uint8_t *)d->buf, d->buf_len * 2);
        d->buf_len = 0;
    }
}
