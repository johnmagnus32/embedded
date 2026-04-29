#ifndef I2S_SINK_H
#define I2S_SINK_H

#include <stdint.h>

typedef void (*i2s_write_fn)(void *opaque, int16_t left, int16_t right);

struct i2s_sink {
    i2s_write_fn write;
    void        *opaque;
};

#endif
