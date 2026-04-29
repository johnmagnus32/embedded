#ifndef MAX98357A_H
#define MAX98357A_H

#include <stdint.h>
#include "i2s_sink.h"
#include "chardev.h"

#define MAX98357A_BUF_SAMPLES 1024

struct max98357a {
    int16_t        buf[MAX98357A_BUF_SAMPLES * 2]; /* interleaved L,R */
    int            count;                           /* sample pairs buffered */
    struct chardev *cd;
};

void max98357a_init(struct max98357a *d, struct chardev *cd);
void max98357a_write(void *opaque, int16_t left, int16_t right);

#endif
