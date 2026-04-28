/*
 * spi_bus.c — Generic SPI bus with multi-slave support
 */
#include <string.h>
#include "spi_bus.h"

void spi_bus_init(struct spi_bus *bus)
{
    memset(bus, 0, sizeof(*bus));
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
