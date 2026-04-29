/*
 * display.h — Display driver API
 */

#ifndef DRIVERS_DISPLAY_H
#define DRIVERS_DISPLAY_H

#include "device.h"
#include <stdint.h>

struct display_driver_api {
    void (*fill_rect)(const struct device *dev, uint16_t x, uint16_t y,
                      uint16_t w, uint16_t h, uint16_t color);
    void (*set_rotation)(const struct device *dev, uint8_t rotation);
    void (*vsync)(const struct device *dev);
};

static inline void display_fill_rect(const struct device *dev,
                                     uint16_t x, uint16_t y,
                                     uint16_t w, uint16_t h, uint16_t color)
{
    const struct display_driver_api *api = dev->api;
    api->fill_rect(dev, x, y, w, h, color);
}

static inline void display_set_rotation(const struct device *dev, uint8_t rotation)
{
    const struct display_driver_api *api = dev->api;
    api->set_rotation(dev, rotation);
}

static inline void display_vsync(const struct device *dev)
{
    const struct display_driver_api *api = dev->api;
    api->vsync(dev);
}

/* RGB565 color helpers */
#define RGB565(r,g,b) ((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|(((b)&0xF8)>>3))

#endif
