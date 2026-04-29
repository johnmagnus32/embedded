/*
 * dma.h — DMA driver API
 */

#ifndef DRIVERS_DMA_H
#define DRIVERS_DMA_H

#include "device.h"
#include <stdint.h>

/* Transfer direction */
#define DMA_MEM_TO_PERIPH  0
#define DMA_PERIPH_TO_MEM  1
#define DMA_MEM_TO_MEM     2

/* Callback status */
#define DMA_STATUS_COMPLETE       0
#define DMA_STATUS_HALF_COMPLETE  1

typedef void (*dma_callback_t)(const struct device *dev, uint8_t stream,
                               int status, void *user_data);

struct dma_channel_config {
    uint8_t  stream;
    uint8_t  channel;       /* CHSEL field */
    uint8_t  direction;     /* DMA_MEM_TO_PERIPH etc */
    uint8_t  periph_size;   /* 0=8bit, 1=16bit, 2=32bit */
    uint8_t  mem_size;      /* 0=8bit, 1=16bit, 2=32bit */
    uint8_t  mem_inc;       /* 1 = increment memory address */
    uint8_t  circular;      /* 1 = circular mode */
    uint32_t periph_addr;
    uint32_t mem_addr;
    uint32_t count;         /* number of data items */
    dma_callback_t callback;
    void    *user_data;
};

struct dma_driver_api {
    int (*configure)(const struct device *dev, const struct dma_channel_config *cfg);
    int (*start)(const struct device *dev, uint8_t stream);
    int (*stop)(const struct device *dev, uint8_t stream);
};

static inline int dma_configure(const struct device *dev,
                                const struct dma_channel_config *cfg)
{
    const struct dma_driver_api *api = dev->api;
    return api->configure(dev, cfg);
}

static inline int dma_start(const struct device *dev, uint8_t stream)
{
    const struct dma_driver_api *api = dev->api;
    return api->start(dev, stream);
}

static inline int dma_stop(const struct device *dev, uint8_t stream)
{
    const struct dma_driver_api *api = dev->api;
    return api->stop(dev, stream);
}

#endif
