#ifndef SPI_H
#define SPI_H

#include <stdint.h>

typedef uint8_t (*spi_transfer_fn)(void *dev, uint8_t byte);
typedef void (*spi_cs_fn)(void *dev, int active);

struct spi_slave {
    void *dev;
    spi_transfer_fn transfer;
    spi_cs_fn cs;
};

struct spi {
    uint32_t cr1, cr2, sr;
    struct spi_slave slave;
};

void     spi_init(struct spi *s);
void     spi_attach(struct spi *s, void *dev, spi_transfer_fn xfer, spi_cs_fn cs);
uint32_t spi_read(void *opaque, uint32_t offset);
void     spi_write(void *opaque, uint32_t offset, uint32_t val);

#endif
