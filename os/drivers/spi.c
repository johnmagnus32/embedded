/*
 * spi.c — STM32 SPI bus driver
 *
 * All config comes from the device tree:
 *   - SPI peripheral base address (reg)
 *   - Clock enable bit (clocks)
 *   - Pin assignments (sck/miso/mosi port, pin, AF)
 *   - CS pin (from child node's cs-port/cs-pin)
 *
 * No hardcoded addresses in this driver.
 * Maps to Zephyr's drivers/spi/spi_ll_stm32.c
 */

#include <stdint.h>
#include "devicetree.h"
#include "device.h"
#include "drivers/spi.h"
#include "drivers/clock.h"

/* SPI register offsets */
#define SPI_CR1_OFF  0x00
#define SPI_CR2_OFF  0x04
#define SPI_SR_OFF   0x08
#define SPI_DR_OFF   0x0C

#define REG(base, off) (*(volatile uint32_t *)((base) + (off)))

struct spi_stm32_config {
    uint32_t base;          /* SPI peripheral base */
    uint8_t  clk_bus;       /* 0=AHB1, 1=APB1, 2=APB2 */
    uint8_t  clk_bit;
    /* Pin config */
    uint32_t sck_port;
    uint8_t  sck_pin;
    uint8_t  sck_af;
    uint32_t miso_port;
    uint8_t  miso_pin;
    uint8_t  miso_af;
    uint32_t mosi_port;
    uint8_t  mosi_pin;
    uint8_t  mosi_af;
    /* CS pin (from flash child node) */
    uint32_t cs_port;
    uint8_t  cs_pin;
};

/* Configure one pin as alternate function */
static void pin_configure_af(uint32_t port, uint8_t pin, uint8_t af)
{
    volatile uint32_t *moder = (volatile uint32_t *)(port + 0x00);
    volatile uint32_t *afr = (volatile uint32_t *)(port + (pin < 8 ? 0x20 : 0x24));
    uint8_t af_pos = (pin % 8) * 4;

    *moder &= ~(3U << (pin * 2));
    *moder |=  (2U << (pin * 2));   /* AF mode */
    *afr &= ~(0xFU << af_pos);
    *afr |=  ((uint32_t)af << af_pos);
}

DEVICE_DT_DECLARE(rcc);

static int spi_stm32_init(const struct device *dev)
{
    const struct spi_stm32_config *cfg = dev->config;
    const struct device *clk = DEVICE_DT_GET(rcc);

    /* Enable SPI peripheral clock */
    clock_on(clk, cfg->clk_bus, cfg->clk_bit);

    /* Enable GPIO clocks for all used pins */
    /* (In a full implementation, each pin's port clock would be
     *  looked up from the DT. Simplified: the GPIO driver already
     *  enables port clocks when pins are configured.) */

    /* Configure SPI pins */
    pin_configure_af(cfg->sck_port, cfg->sck_pin, cfg->sck_af);
    pin_configure_af(cfg->miso_port, cfg->miso_pin, cfg->miso_af);
    pin_configure_af(cfg->mosi_port, cfg->mosi_pin, cfg->mosi_af);

    /* Configure CS as GPIO output, start high */
    volatile uint32_t *cs_moder = (volatile uint32_t *)(cfg->cs_port + 0x00);
    *cs_moder &= ~(3U << (cfg->cs_pin * 2));
    *cs_moder |=  (1U << (cfg->cs_pin * 2));  /* output */
    volatile uint32_t *cs_bsrr = (volatile uint32_t *)(cfg->cs_port + 0x18);
    *cs_bsrr = (1 << cfg->cs_pin);  /* set high (deselected) */

    /* Configure SPI: master, 8-bit, CPOL=0, CPHA=0, fPCLK/4 */
    REG(cfg->base, SPI_CR1_OFF) =
        (1 << 2)   /* MSTR */
      | (1 << 3)   /* BR = 001 → fPCLK/4 */
      | (1 << 8)   /* SSI */
      | (1 << 9);  /* SSM */
    REG(cfg->base, SPI_CR1_OFF) |= (1 << 6);  /* SPE: enable */

    return 0;
}

static uint8_t spi_xfer_byte(uint32_t base, uint8_t tx)
{
    while (!(REG(base, SPI_SR_OFF) & (1 << 1)));  /* wait TXE */
    REG(base, SPI_DR_OFF) = tx;
    while (!(REG(base, SPI_SR_OFF) & (1 << 0)));  /* wait RXNE */
    return (uint8_t)REG(base, SPI_DR_OFF);
}

static int spi_stm32_transceive(const struct device *dev,
                                const uint8_t *tx, size_t tx_len,
                                uint8_t *rx, size_t rx_len)
{
    const struct spi_stm32_config *cfg = dev->config;
    size_t total = tx_len > rx_len ? tx_len : rx_len;
    /* If both provided, total is the larger */
    if (tx && rx) total = tx_len + rx_len;

    size_t i = 0;
    /* Phase 1: send TX bytes, discard received */
    for (; i < tx_len; i++)
        spi_xfer_byte(cfg->base, tx ? tx[i] : 0xFF);

    /* Phase 2: clock out dummy bytes, capture RX */
    for (size_t j = 0; j < rx_len; j++)
        rx[j] = spi_xfer_byte(cfg->base, 0xFF);

    return 0;
}

static void spi_stm32_cs_select(const struct device *dev)
{
    const struct spi_stm32_config *cfg = dev->config;
    volatile uint32_t *bsrr = (volatile uint32_t *)(cfg->cs_port + 0x18);
    *bsrr = (1 << (cfg->cs_pin + 16));  /* reset = low = selected */
}

static void spi_stm32_cs_release(const struct device *dev)
{
    const struct spi_stm32_config *cfg = dev->config;
    volatile uint32_t *bsrr = (volatile uint32_t *)(cfg->cs_port + 0x18);
    *bsrr = (1 << cfg->cs_pin);  /* set = high = deselected */
}

static const struct spi_driver_api spi_stm32_api = {
    .transceive = spi_stm32_transceive,
    .cs_select = spi_stm32_cs_select,
    .cs_release = spi_stm32_cs_release,
};

/* ---- DT_INST instantiation ---- */

#define _SPI_INST_LABEL(n) DT_INST_ST_STM32_SPI_##n##_LABEL
#define _SPI_PROP(n, p) DT_INST_ST_STM32_SPI_##n##_PROP_##p
#define _SPI_CHILD(n, p) DT_INST_ST_STM32_SPI_##n##_CHILD_0_##p

#define STM32_SPI_DEFINE(n)                                         \
    static const struct spi_stm32_config spi_cfg_##n = {            \
        .base      = DT_INST_ST_STM32_SPI_##n##_REG_ADDR,          \
        .clk_bus   = DT_INST_ST_STM32_SPI_##n##_CLK_BUS,           \
        .clk_bit   = DT_INST_ST_STM32_SPI_##n##_CLK_BIT,           \
        .sck_port  = _SPI_PROP(n, SCK_PORT_BASE),                  \
        .sck_pin   = _SPI_PROP(n, SCK_PIN),                        \
        .sck_af    = _SPI_PROP(n, SCK_AF),                         \
        .miso_port = _SPI_PROP(n, MISO_PORT_BASE),                 \
        .miso_pin  = _SPI_PROP(n, MISO_PIN),                       \
        .miso_af   = _SPI_PROP(n, MISO_AF),                        \
        .mosi_port = _SPI_PROP(n, MOSI_PORT_BASE),                 \
        .mosi_pin  = _SPI_PROP(n, MOSI_PIN),                       \
        .mosi_af   = _SPI_PROP(n, MOSI_AF),                        \
        .cs_port   = _SPI_CHILD(n, CS_PORT_BASE),                  \
        .cs_pin    = _SPI_CHILD(n, CS_PIN),                        \
    };                                                              \
    DEVICE_DT_DEFINE(_SPI_INST_LABEL(n),                            \
                     spi_stm32_init, NULL, &spi_cfg_##n,            \
                     &spi_stm32_api);

DT_INST_FOREACH_STATUS_OKAY(ST_STM32_SPI, STM32_SPI_DEFINE)
