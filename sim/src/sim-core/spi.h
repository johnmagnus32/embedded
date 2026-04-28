#ifndef SPI_H
#define SPI_H

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

struct spi {
    uint32_t cr1, cr2, sr;
    struct spi_bus bus;
};

void     spi_init(struct spi *s);
int      spi_bus_attach(struct spi_bus *bus, void *dev, spi_transfer_fn xfer);
uint8_t  spi_bus_transfer(struct spi_bus *bus, uint8_t byte);
void     spi_slave_cs_handler(void *opaque, int level);
uint32_t spi_read(void *opaque, uint32_t offset);
void     spi_write(void *opaque, uint32_t offset, uint32_t val);

#endif
