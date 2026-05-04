#ifndef STM32_SPI_H
#define STM32_SPI_H

#include <stdint.h>
#include "spi_bus.h"
#include "i2s_sink.h"
#include "event_queue.h"

struct stm32_dma_stream;

struct stm32_spi {
    uint32_t cr1, cr2, sr;
    uint8_t  rx_data;             /* last received byte from MISO */
    struct spi_bus bus;
    /* SPI clock pacing */
    uint32_t        spi_cycles_per_byte;  /* CPU cycles per SPI byte transfer */
    uint32_t        spi_cycle_counter;    /* countdown to TXE re-assertion */
    int             spi_active;           /* non-zero when a transfer is in progress */
    /* I2S extension */
    uint32_t        i2scfgr, i2spr;
    int             i2s_mode;
    struct stm32_dma_stream *dma_tx;
    struct event_queue *eq;        /* for scheduling TXE events */
    uint64_t           *cycle_ptr; /* pointer to cpu->cycle_count */
    int                 spi_evt_id; /* event ID for this SPI's TXE */
    /* I2S sample rate pacing */
    uint32_t        i2s_cycles_per_sample;
    uint32_t        i2s_cycle_counter;
    /* I2S stereo tracking */
    int             i2s_lr;
    int16_t         i2s_pending_left;
    struct i2s_sink *i2s_sink;
};

void     stm32_spi_init(struct stm32_spi *s);
uint32_t stm32_spi_read(void *opaque, uint32_t offset);
void     stm32_spi_write(void *opaque, uint32_t offset, uint32_t val);
void     stm32_spi_tick(struct stm32_spi *s);

#endif
