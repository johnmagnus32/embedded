/*
 * spi.c — STM32 SPI peripheral emulation
 *
 * Minimal: CR1 (enable, master), SR (TXE/RXNE/BSY), DR (data).
 * When firmware writes DR, the byte is forwarded to the attached slave.
 */
#include "spi.h"

/* STM32 SPI register offsets */
#define SPI_CR1  0x00
#define SPI_CR2  0x04
#define SPI_SR   0x08
#define SPI_DR   0x0C

/* SR bits */
#define SR_TXE   (1 << 1)
#define SR_RXNE  (1 << 0)
#define SR_BSY   (1 << 7)

void spi_init(struct spi *s, uint32_t base)
{
    s->base = base;
    s->cr1 = 0;
    s->cr2 = 0;
    s->sr = SR_TXE; /* TX empty on reset */
    s->slave = 0;
    s->slave_transfer = 0;
    s->slave_cs = 0;
}

void spi_attach(struct spi *s, void *dev, spi_transfer_fn xfer, spi_cs_fn cs)
{
    s->slave = dev;
    s->slave_transfer = xfer;
    s->slave_cs = cs;
}

int spi_handles(struct spi *s, uint32_t addr)
{
    return addr >= s->base && addr < s->base + 0x24;
}

uint32_t spi_read(struct spi *s, uint32_t addr)
{
    switch (addr - s->base) {
    case SPI_CR1: return s->cr1;
    case SPI_CR2: return s->cr2;
    case SPI_SR:  return s->sr;
    case SPI_DR:  s->sr &= ~SR_RXNE; return 0;
    default:      return 0;
    }
}

void spi_write(struct spi *s, uint32_t addr, uint32_t val)
{
    switch (addr - s->base) {
    case SPI_CR1: s->cr1 = val; break;
    case SPI_CR2: s->cr2 = val; break;
    case SPI_DR:
        if (s->slave_transfer)
            s->slave_transfer(s->slave, (uint8_t)val);
        s->sr |= SR_TXE | SR_RXNE;
        s->sr &= ~SR_BSY;
        break;
    }
}
