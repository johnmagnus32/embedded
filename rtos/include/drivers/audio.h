/*
 * audio.h — Audio driver API (buffer submission model)
 *
 * The app loops: get_buffer → fill → put_buffer → repeat.
 * No callbacks, no ISR concerns in application code.
 */

#ifndef DRIVERS_AUDIO_H
#define DRIVERS_AUDIO_H

#include "device.h"
#include <stdint.h>

#define AUDIO_BUF_SAMPLES 128  /* samples per half-buffer (stereo pairs) */

struct audio_driver_api {
    int  (*start)(const struct device *dev);
    int  (*stop)(const struct device *dev);
    int16_t *(*get_buffer)(const struct device *dev);
    void (*put_buffer)(const struct device *dev, int16_t *buf);
};

static inline int audio_start(const struct device *dev)
{
    const struct audio_driver_api *api = dev->api;
    return api->start(dev);
}

static inline int audio_stop(const struct device *dev)
{
    const struct audio_driver_api *api = dev->api;
    return api->stop(dev);
}

static inline int16_t *audio_get_buffer(const struct device *dev)
{
    const struct audio_driver_api *api = dev->api;
    return api->get_buffer(dev);
}

static inline void audio_put_buffer(const struct device *dev, int16_t *buf)
{
    const struct audio_driver_api *api = dev->api;
    api->put_buffer(dev, buf);
}

#endif
