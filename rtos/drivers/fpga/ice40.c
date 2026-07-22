/*
 * ice40.c — Lattice iCE40UP5K bitstream loader (SPI-slave configuration)
 *
 * Configures an iCE40UP5K over the iCE40 SPI-slave procedure (Lattice TN1248):
 * the MCU is the sole config master, streaming the bitstream in at boot. This
 * is the driver-model version of the loader that fpga-blink / fpga-spi bit-bang
 * by hand; here the byte clocking goes through the SPI driver and the three
 * control lines through the GPIO driver — no hardcoded register addresses.
 *
 * Wiring comes entirely from the device tree (ice40-cfg node):
 *   - the SPI bus is the DTS parent node (DT_INST..._PARENT_LABEL)
 *   - creset-port/pin, cdone-port/pin, ss-port/pin give the control lines
 *
 * Config sequence:
 *   1. CRESET_B low  + SPI_SS_B low   (strap slave mode; SS sampled at CRESET release)
 *   2. hold ~1 us
 *   3. CRESET_B high, wait 1200 us    (config-memory clear)
 *   4. clock the bitstream (mode 0, MSB first) over the SPI bus
 *   5. >=49 trailing clocks (send 7 dummy bytes = 56 clocks)
 *   6. SPI_SS_B high                  (FPGA enters user mode)
 *   7. CDONE high == success
 *
 * Maps to the shape of Zephyr's SPI-configured FPGA loaders (drivers/misc).
 */
#include <stdint.h>
#include <stddef.h>
#include "devicetree.h"
#include "device.h"
#include "drivers/fpga.h"
#include "drivers/spi.h"
#include "drivers/gpio.h"

/* The SPI bus (config bus) and the GPIO port carrying the control lines.
 * Resolved by the device model; declared here like the flash/display drivers.
 * (CRESET/CDONE/SS all live on the same GPIO port in this design.) */
DEVICE_DT_DECLARE(DT_INST_LATTICE_ICE40UP5K_CFG_0_PARENT_LABEL);
DEVICE_DT_DECLARE(gpiob);

struct ice40_config {
    uint8_t creset_pin;
    uint8_t cdone_pin;
    uint8_t ss_pin;
};

struct ice40_data {
    const struct device *spi;
    const struct device *creset_gpio;
    const struct device *cdone_gpio;
    const struct device *ss_gpio;
};

/* Simple busy-wait; config runs once at boot before the scheduler starts, so a
 * calibrated spin (same idiom as the ILI9341 driver) is adequate. Tuned for the
 * ~96 MHz M4 core this board runs; a few % error on the 1200 us wait is fine
 * (TN1248 specifies a *minimum*). */
static void ice40_delay_us(uint32_t us)
{
    for (volatile uint32_t i = 0; i < us * 24u; i++)
        __asm__ volatile ("");
}

static int ice40_load(const struct device *dev,
                      const uint8_t *bitstream, size_t len)
{
    const struct ice40_config *cfg = dev->config;
    struct ice40_data *data = dev->data;

    /* CRESET_B low + SPI_SS_B low: clear config SRAM and strap slave mode.
     * SS drives low directly here (we own it as a plain GPIO during config,
     * not as the SPI peripheral's hardware CS). */
    gpio_pin_set(data->creset_gpio, cfg->creset_pin, 0);
    gpio_pin_set(data->ss_gpio, cfg->ss_pin, 0);
    ice40_delay_us(1);

    /* Release CRESET; SS stays low. Wait for config-memory clear. */
    gpio_pin_set(data->creset_gpio, cfg->creset_pin, 1);
    ice40_delay_us(1200);

    /* Stream the bitstream, then trailing clocks, all with SS held low.
     * spi_write drives the same MOSI/SCK the FPGA samples on rising edges. */
    spi_write(data->spi, bitstream, len);

    static const uint8_t trailing[7] = {0}; /* 56 clocks >= 49 minimum */
    spi_write(data->spi, trailing, sizeof(trailing));

    /* Release SS: FPGA transitions to user mode. */
    gpio_pin_set(data->ss_gpio, cfg->ss_pin, 1);
    ice40_delay_us(100);

    /* CDONE high == configuration succeeded. */
    return gpio_pin_get(data->cdone_gpio, cfg->cdone_pin) ? 0 : -1;
}

static const struct fpga_driver_api ice40_api = {
    .load = ice40_load,
};

static int ice40_init(const struct device *dev)
{
    const struct ice40_config *cfg = dev->config;
    struct ice40_data *data = dev->data;

    data->spi         = DEVICE_DT_GET(DT_INST_LATTICE_ICE40UP5K_CFG_0_PARENT_LABEL);
    data->creset_gpio = DEVICE_DT_GET(gpiob);
    data->cdone_gpio  = DEVICE_DT_GET(gpiob);
    data->ss_gpio     = DEVICE_DT_GET(gpiob);

    /* CRESET_B, SPI_SS_B = outputs; CDONE = input, pull-down so a floating pin
     * reads 0 and only a driven-high (configured) CDONE reads 1. */
    gpio_pin_configure(data->creset_gpio, cfg->creset_pin, GPIO_OUTPUT);
    gpio_pin_configure(data->ss_gpio, cfg->ss_pin, GPIO_OUTPUT);
    gpio_pin_configure(data->cdone_gpio, cfg->cdone_pin, GPIO_INPUT | GPIO_PULL_DOWN);

    /* Idle high until a load() drives them; keeps the FPGA out of reset. */
    gpio_pin_set(data->creset_gpio, cfg->creset_pin, 1);
    gpio_pin_set(data->ss_gpio, cfg->ss_pin, 1);
    return 0;
}

/* ---- DT_INST instantiation ---- */

#define ICE40_DEFINE(n)                                                     \
    static const struct ice40_config ice40_cfg_##n = {                      \
        .creset_pin = DT_INST_LATTICE_ICE40UP5K_CFG_##n##_PROP_CRESET_PIN,  \
        .cdone_pin  = DT_INST_LATTICE_ICE40UP5K_CFG_##n##_PROP_CDONE_PIN,   \
        .ss_pin     = DT_INST_LATTICE_ICE40UP5K_CFG_##n##_PROP_SS_PIN,      \
    };                                                                      \
    static struct ice40_data ice40_data_##n;                                \
    DEVICE_DT_DEFINE(DT_INST_LATTICE_ICE40UP5K_CFG_##n##_LABEL,             \
                     ice40_init, &ice40_data_##n,                           \
                     &ice40_cfg_##n, &ice40_api, 30);

DT_INST_FOREACH_STATUS_OKAY(LATTICE_ICE40UP5K_CFG, ICE40_DEFINE)
