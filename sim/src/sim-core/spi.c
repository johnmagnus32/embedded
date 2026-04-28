/*
 * spi.c — STM32 SPI peripheral emulation
 */
#include "spi.h"

#define SPI_CR1  0x00
#define SPI_CR2  0x04
#define SPI_SR   0x08
#define SPI_DR   0x0C

#define SR_TXE   (1 << 1)
#define SR_RXNE  (1 << 0)
#define SR_BSY   (1 << 7)

void spi_init(struct spi *s)
{
    s->cr1 = 0;
    s->cr2 = 0;
    s->sr = SR_TXE;
    s->slave.dev = 0;
    s->slave.transfer = 0;
    s->slave.cs = 0;
}

void spi_attach(struct spi *s, void *dev, spi_transfer_fn xfer, spi_cs_fn cs)
{
    s->slave.dev = dev;
    s->slave.transfer = xfer;
    s->slave.cs = cs;
}

uint32_t spi_read(void *opaque, uint32_t offset)
{
    struct spi *s = (struct spi *)opaque;
    switch (offset) {
    case SPI_CR1: return s->cr1;
    case SPI_CR2: return s->cr2;
    case SPI_SR:  return s->sr;
    case SPI_DR:  s->sr &= ~SR_RXNE; return 0;
    default:      return 0;
    }
}

void spi_write(void *opaque, uint32_t offset, uint32_t val)
{
    struct spi *s = (struct spi *)opaque;
    switch (offset) {
    case SPI_CR1: s->cr1 = val; break;
    case SPI_CR2: s->cr2 = val; break;
    case SPI_DR:
        if (s->slave.transfer)
            s->slave.transfer(s->slave.dev, (uint8_t)val);
        s->sr |= SR_TXE | SR_RXNE;
        s->sr &= ~SR_BSY;
        break;
    }
}
