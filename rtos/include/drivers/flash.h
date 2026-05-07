/*
 * flash.h — NOR flash driver API
 *
 * Like Zephyr's struct flash_driver_api.
 * Provides erase/write/read operations.
 *
 * NOR flash rules:
 *   - Must erase before writing (erase sets all bits to 1)
 *   - Erase granularity = sector (typically 4KB)
 *   - Write granularity = page (typically 256 bytes)
 *   - Read has no restrictions
 */

#ifndef DRIVERS_FLASH_H
#define DRIVERS_FLASH_H

#include "device.h"
#include <stddef.h>
#include <stdint.h>

struct flash_driver_api {
    int (*read)(const struct device *dev, uint32_t offset, void *buf, size_t len);
    int (*write)(const struct device *dev, uint32_t offset, const void *buf, size_t len);
    int (*erase)(const struct device *dev, uint32_t offset, size_t size);
};

static inline int flash_read(const struct device *dev, uint32_t offset,
                             void *buf, size_t len)
{
    const struct flash_driver_api *api = dev->api;
    return api->read(dev, offset, buf, len);
}

static inline int flash_write(const struct device *dev, uint32_t offset,
                              const void *buf, size_t len)
{
    const struct flash_driver_api *api = dev->api;
    return api->write(dev, offset, buf, len);
}

static inline int flash_erase(const struct device *dev, uint32_t offset,
                              size_t size)
{
    const struct flash_driver_api *api = dev->api;
    return api->erase(dev, offset, size);
}

#endif
