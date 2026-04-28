#ifndef SPI_BUS_H
#define SPI_BUS_H

#include <stdint.h>

#define SPI_MAX_SLAVES 4

typedef uint8_t (*spi_transfer_fn)(void *dev, uint8_t byte);

struct spi_slave {
    void *dev;
    spi_transfer_fn transfer;
    int cs_active;
};

struct spi_bus {
    struct spi_slave slaves[SPI_MAX_SLAVES];
    int nslaves;
};

int     spi_bus_attach(struct spi_bus *bus, void *dev, spi_transfer_fn xfer);
uint8_t spi_bus_transfer(struct spi_bus *bus, uint8_t byte);

#endif
