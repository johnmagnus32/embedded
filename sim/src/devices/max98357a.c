#include "max98357a.h"
#include <string.h>

void max98357a_init(struct max98357a *d, struct chardev *cd)
{
    memset(d, 0, sizeof(*d));
    d->cd = cd;
}

void max98357a_write(void *opaque, int16_t left, int16_t right)
{
    struct max98357a *d = (struct max98357a *)opaque;
    int i = d->count * 2;
    d->buf[i]     = left;
    d->buf[i + 1] = right;
    d->count++;
    if (d->count >= 256) {
        if (d->cd) {
            chardev_try_accept(d->cd);
            chardev_write_buf(d->cd, (const uint8_t *)d->buf, d->count * 4);
        }
        d->count = 0;
    }
}
