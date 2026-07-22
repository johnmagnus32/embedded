/*
 * fpga.h — FPGA bitstream-loader driver API
 *
 * A small Zephyr-style driver subsystem for FPGAs that are configured by the
 * MCU at boot (their configuration SRAM is volatile and must be reloaded every
 * power-up). The MCU streams a bitstream into the FPGA over a bus the driver
 * owns; the concrete protocol lives in the per-device driver.
 *
 * Current backend: rtos/drivers/fpga/ice40.c — Lattice iCE40UP5K SPI-slave
 * configuration (TN1248), driving CRESET_B / SPI_SS_B / CDONE via the GPIO
 * driver and clocking the bitstream over the SPI driver.
 */
#ifndef DRIVERS_FPGA_H
#define DRIVERS_FPGA_H

#include "device.h"
#include <stdint.h>
#include <stddef.h>

struct fpga_driver_api {
    /* Stream `len` bytes of `bitstream` into the FPGA and bring it up.
     * Returns 0 on success (FPGA reports configured), negative on failure. */
    int (*load)(const struct device *dev, const uint8_t *bitstream, size_t len);
};

/* Load a bitstream into the FPGA. Returns 0 on success, <0 on failure. */
static inline int fpga_load(const struct device *dev,
                            const uint8_t *bitstream, size_t len)
{
    const struct fpga_driver_api *api = dev->api;
    return api->load(dev, bitstream, len);
}

#endif /* DRIVERS_FPGA_H */
