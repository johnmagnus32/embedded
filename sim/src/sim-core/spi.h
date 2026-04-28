#ifndef SPI_H
#define SPI_H

#include <stdint.h>

/* Forward declaration — any SPI slave device implements this */
typedef uint8_t (*spi_transfer_fn)(void *dev, uint8_t byte);
typedef void (*spi_cs_fn)(void *dev, int active);

struct spi {
    uint32_t base;
    uint32_t cr1, cr2, sr;
    /* Attached device */
    void *slave;
    spi_transfer_fn slave_transfer;
    spi_cs_fn slave_cs;
};

void     spi_init(struct spi *s, uint32_t base);
void     spi_attach(struct spi *s, void *dev, spi_transfer_fn xfer, spi_cs_fn cs);
int      spi_handles(struct spi *s, uint32_t addr);
uint32_t spi_read(struct spi *s, uint32_t addr);
void     spi_write(struct spi *s, uint32_t addr, uint32_t val);

#endif
