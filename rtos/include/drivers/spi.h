/*
 * spi.h — SPI bus driver API
 *
 * Like Zephyr's struct spi_driver_api.
 * Provides transfer (simultaneous TX/RX) and convenience wrappers.
 */

#ifndef DRIVERS_SPI_H
#define DRIVERS_SPI_H

#include "device.h"
#include <stdint.h>
#include <stddef.h>

struct spi_driver_api {
    /* Transfer: send tx_len bytes, receive rx_len bytes */
    int (*transceive)(const struct device *dev,
                      const uint8_t *tx, size_t tx_len,
                      uint8_t *rx, size_t rx_len);
    void (*cs_select)(const struct device *dev);
    void (*cs_release)(const struct device *dev);
};

/* Flags for spi_write/spi_read */
#define SPI_HOLD_CS  (1 << 0)  /* Keep CS asserted after transfer */

static inline void spi_cs_select(const struct device *dev)
{
    const struct spi_driver_api *api = dev->api;
    api->cs_select(dev);
}

static inline void spi_cs_release(const struct device *dev)
{
    const struct spi_driver_api *api = dev->api;
    api->cs_release(dev);
}

/* Send tx_len bytes, ignore received data */
static inline int spi_write(const struct device *dev,
                            const uint8_t *tx, size_t tx_len)
{
    const struct spi_driver_api *api = dev->api;
    return api->transceive(dev, tx, tx_len, (void *)0, 0);
}

/* Send tx_len bytes with flags (SPI_HOLD_CS to keep CS low) */
static inline int spi_write_f(const struct device *dev,
                              const uint8_t *tx, size_t tx_len, uint8_t flags)
{
    const struct spi_driver_api *api = dev->api;
    api->cs_select(dev);
    int r = api->transceive(dev, tx, tx_len, (void *)0, 0);
    if (!(flags & SPI_HOLD_CS))
        api->cs_release(dev);
    return r;
}

/* Send tx_len bytes of 0xFF, capture received data */
static inline int spi_read(const struct device *dev,
                           uint8_t *rx, size_t rx_len)
{
    const struct spi_driver_api *api = dev->api;
    return api->transceive(dev, (void *)0, 0, rx, rx_len);
}

/* Full duplex: send and receive simultaneously */
static inline int spi_transceive(const struct device *dev,
                                 const uint8_t *tx, size_t tx_len,
                                 uint8_t *rx, size_t rx_len)
{
    const struct spi_driver_api *api = dev->api;
    api->cs_select(dev);
    int r = api->transceive(dev, tx, tx_len, rx, rx_len);
    api->cs_release(dev);
    return r;
}

#endif
