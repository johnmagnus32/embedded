/*
 * audio.h — Audio driver API (I2S output)
 */

#ifndef DRIVERS_AUDIO_H
#define DRIVERS_AUDIO_H

#include "device.h"
#include <stdint.h>

/* Callback: fill buf with count samples. Called from ISR context. */
typedef void (*audio_fill_fn)(int16_t *buf, int count, void *user_data);

struct audio_driver_api {
    void (*write_sample)(const struct device *dev, int16_t left, int16_t right);
    int  (*start)(const struct device *dev, audio_fill_fn fill, void *user_data);
    int  (*stop)(const struct device *dev);
};

static inline void audio_write_sample(const struct device *dev,
                                      int16_t left, int16_t right)
{
    const struct audio_driver_api *api = dev->api;
    api->write_sample(dev, left, right);
}

static inline int audio_start(const struct device *dev,
                              audio_fill_fn fill, void *user_data)
{
    const struct audio_driver_api *api = dev->api;
    return api->start(dev, fill, user_data);
}

static inline int audio_stop(const struct device *dev)
{
    const struct audio_driver_api *api = dev->api;
    return api->stop(dev);
}

#endif
