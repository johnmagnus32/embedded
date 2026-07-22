#ifndef MAX98357A_H
#define MAX98357A_H

#include <stdint.h>
#include "chardev.h"
#include "i2s_sink.h"

struct max98357a {
    struct chardev  *cd;
    struct i2s_sink  sink;
    int16_t          buf[1024];
    int              buf_len;
};

void max98357a_init(struct max98357a *d);

/* Called by I2S sink when a stereo sample is ready */
void max98357a_write(void *opaque, int16_t left, int16_t right);

#endif
