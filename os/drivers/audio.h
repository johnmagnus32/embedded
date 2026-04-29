/*
 * audio.h — Audio driver API (I2S output)
 */

#ifndef DRIVERS_AUDIO_H
#define DRIVERS_AUDIO_H

#include "device.h"
#include <stdint.h>

struct audio_driver_api {
    void (*write_sample)(const struct device *dev, int16_t left, int16_t right);
};

static inline void audio_write_sample(const struct device *dev,
                                      int16_t left, int16_t right)
{
    const struct audio_driver_api *api = dev->api;
    api->write_sample(dev, left, right);
}

#endif
