#ifndef STM32_DMA_H
#define STM32_DMA_H

#include <stdint.h>
#include "armv7m_nvic.h"
#include "membus.h"

struct stm32_dma_stream {
    uint32_t cr, ndtr, par, m0ar, m1ar, fcr;
    uint32_t ndtr_orig, m0ar_orig;
    int      request_pending;
    int     *any_active_ptr;
    int     *any_pending_ptr;
};

struct stm32_dma {
    struct stm32_dma_stream streams[8];
    uint32_t lisr, hisr;
    int      any_active;
    int      any_pending;
    uint8_t  active_mask;
    struct armv7m_nvic *nvic;
    struct membus      *bus;
};

void     stm32_dma_init(struct stm32_dma *d, struct armv7m_nvic *nvic, struct membus *bus);
uint32_t stm32_dma_read(void *opaque, uint32_t offset);
void     stm32_dma_write(void *opaque, uint32_t offset, uint32_t val);
void     stm32_dma_tick(struct stm32_dma *d);

#endif
