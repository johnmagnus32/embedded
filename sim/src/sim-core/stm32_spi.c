/*
 * stm32_spi.c — STM32 SPI peripheral register interface
 */
#include <string.h>
#include "stm32_spi.h"
#include "stm32_dma.h"

#define SPI_CR1  0x00
#define SPI_CR2  0x04
#define SPI_SR   0x08
#define SPI_DR   0x0C
#define SPI_I2SCFGR 0x1C
#define SPI_I2SPR   0x20

#define SR_TXE   (1 << 1)
#define SR_RXNE  (1 << 0)
#define SR_BSY   (1 << 7)

void stm32_spi_init(struct stm32_spi *s)
{
    s->cr1 = 0;
    s->cr2 = 0;
    s->sr = SR_TXE;
    memset(&s->bus, 0, sizeof(s->bus));
    s->i2scfgr = 0;
    s->i2spr = 0;
    s->i2s_mode = 0;
    s->i2s_lr = 0;
    s->i2s_pending_left = 0;
    s->i2s_sink = NULL;
    s->dma_tx = NULL;
}

uint32_t stm32_spi_read(void *opaque, uint32_t offset)
{
    struct stm32_spi *s = (struct stm32_spi *)opaque;
    switch (offset) {
    case SPI_CR1: return s->cr1;
    case SPI_CR2: return s->cr2;
    case SPI_SR:  return s->sr;
    case SPI_DR:  s->sr &= ~SR_RXNE; return 0;
    case SPI_I2SCFGR: return s->i2scfgr;
    case SPI_I2SPR:   return s->i2spr;
    default:      return 0;
    }
}

void stm32_spi_write(void *opaque, uint32_t offset, uint32_t val)
{
    struct stm32_spi *s = (struct stm32_spi *)opaque;
    switch (offset) {
    case SPI_CR1: s->cr1 = val; break;
    case SPI_CR2: s->cr2 = val; break;
    case SPI_DR:
        if (s->i2s_mode) {
            int16_t sample = (int16_t)(val & 0xFFFF);
            if (s->i2s_lr == 0) {
                s->i2s_pending_left = sample;
                s->i2s_lr = 1;
            } else {
                if (s->i2s_sink && s->i2s_sink->write)
                    s->i2s_sink->write(s->i2s_sink->opaque, s->i2s_pending_left, sample);
                s->i2s_lr = 0;
            }
            s->sr |= SR_TXE;
            /* Re-assert DMA request when TXE and TXDMAEN */
            if ((s->cr2 & (1 << 1)) && s->dma_tx)
                s->dma_tx->request_pending = 1;
        } else {
            spi_bus_transfer(&s->bus, (uint8_t)val);
            s->sr |= SR_TXE | SR_RXNE;
            s->sr &= ~SR_BSY;
        }
        break;
    case SPI_I2SCFGR:
        s->i2scfgr = val;
        s->i2s_mode = (val & (1 << 11)) ? 1 : 0; /* I2SMOD bit */
        /* If enabling I2S with TXDMAEN, kick off first DMA transfer */
        if (s->i2s_mode && (val & (1 << 10)) && (s->cr2 & (1 << 1)) && s->dma_tx)
            s->dma_tx->request_pending = 1;
        break;
    case SPI_I2SPR:
        s->i2spr = val;
        break;
    }
}
