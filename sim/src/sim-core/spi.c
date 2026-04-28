/*
 * spi.c — STM32 SPI peripheral emulation with multi-slave bus
 */
#include <string.h>
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
    memset(&s->bus, 0, sizeof(s->bus));
}

int spi_bus_attach(struct spi_bus *bus, void *dev, spi_transfer_fn xfer)
{
    if (bus->nslaves >= SPI_MAX_SLAVES)
        return -1;
    int idx = bus->nslaves++;
    bus->slaves[idx].dev = dev;
    bus->slaves[idx].transfer = xfer;
    bus->slaves[idx].cs_active = 0;
    return idx;
}

uint8_t spi_bus_transfer(struct spi_bus *bus, uint8_t byte)
{
    uint8_t ret = 0;
    for (int i = 0; i < bus->nslaves; i++) {
        if (bus->slaves[i].cs_active && bus->slaves[i].transfer)
            ret = bus->slaves[i].transfer(bus->slaves[i].dev, byte);
    }
    return ret;
}

void spi_slave_cs_handler(void *opaque, int level)
{
    struct spi_slave *slave = (struct spi_slave *)opaque;
    slave->cs_active = !level;  /* CS is active low */
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
        spi_bus_transfer(&s->bus, (uint8_t)val);
        s->sr |= SR_TXE | SR_RXNE;
        s->sr &= ~SR_BSY;
        break;
    }
}
