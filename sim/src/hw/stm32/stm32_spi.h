#ifndef STM32_SPI_H
#define STM32_SPI_H

#include <stdint.h>
#include "spi_bus.h"

struct stm32_dma_stream;

struct stm32_spi {
    uint32_t cr1, cr2, sr;
    struct spi_bus bus;
    /* I2S extension */
    uint32_t        i2scfgr, i2spr;
    int             i2s_mode;
    struct stm32_dma_stream *dma_tx;
};

void     stm32_spi_init(struct stm32_spi *s);
uint32_t stm32_spi_read(void *opaque, uint32_t offset);
void     stm32_spi_write(void *opaque, uint32_t offset, uint32_t val);

#endif
