/*
 * flash.c — W25Q128 SPI NOR flash driver
 *
 * Talks to a real Winbond W25Q128JV over SPI.
 * Uses the STM32 SPI1 peripheral (PA5=SCK, PA7=MOSI, PA6=MISO, PA4=CS).
 *
 * W25Q128 command set:
 *   0x9F  JEDEC ID (read 3 bytes: manufacturer, memory type, capacity)
 *   0x06  Write Enable (WREN) — must send before program/erase
 *   0x04  Write Disable
 *   0x05  Read Status Register 1 (bit 0 = BUSY)
 *   0x03  Read Data (up to entire chip, continuous)
 *   0x02  Page Program (max 256 bytes, must be within one page)
 *   0x20  Sector Erase (4KB)
 *   0xD8  Block Erase (64KB)
 *   0xC7  Chip Erase
 */

#include <stdint.h>
#include "devicetree.h"
#include "device.h"
#include "drivers/flash.h"

/* --- SPI1 registers (STM32F411) --- */
#define SPI1_BASE   0x40013000
#define SPI_CR1     (*(volatile uint32_t *)(SPI1_BASE + 0x00))
#define SPI_CR2     (*(volatile uint32_t *)(SPI1_BASE + 0x04))
#define SPI_SR      (*(volatile uint32_t *)(SPI1_BASE + 0x08))
#define SPI_DR      (*(volatile uint32_t *)(SPI1_BASE + 0x0C))

/* --- GPIO for CS pin (PA4) --- */
#define GPIOA_BASE  0x40020000
#define GPIOA_MODER (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_BSRR  (*(volatile uint32_t *)(GPIOA_BASE + 0x18))
#define GPIOA_AFRL  (*(volatile uint32_t *)(GPIOA_BASE + 0x20))

/* RCC */
#define RCC_BASE    0x40023800
#define RCC_AHB1ENR (*(volatile uint32_t *)(RCC_BASE + 0x30))
#define RCC_APB2ENR (*(volatile uint32_t *)(RCC_BASE + 0x44))

/* CS pin control */
#define CS_LOW()    (GPIOA_BSRR = (1 << (4 + 16)))  /* reset PA4 */
#define CS_HIGH()   (GPIOA_BSRR = (1 << 4))          /* set PA4 */

/* W25Q128 commands */
#define CMD_WRITE_ENABLE  0x06
#define CMD_READ_STATUS   0x05
#define CMD_READ_DATA     0x03
#define CMD_PAGE_PROGRAM  0x02
#define CMD_SECTOR_ERASE  0x20
#define CMD_JEDEC_ID      0x9F

#define STATUS_BUSY       0x01

struct flash_nor_config {
    uint32_t size;
    uint16_t page_size;
    uint16_t sector_size;
};

/* --- Low-level SPI --- */

static void spi_init(void)
{
    /* Enable clocks: GPIOA + SPI1 */
    RCC_AHB1ENR |= (1 << 0);   /* GPIOA */
    RCC_APB2ENR |= (1 << 12);  /* SPI1 */

    /* PA5 = SPI1_SCK (AF5), PA6 = SPI1_MISO (AF5), PA7 = SPI1_MOSI (AF5) */
    GPIOA_MODER &= ~((3 << 10) | (3 << 12) | (3 << 14));
    GPIOA_MODER |=  ((2 << 10) | (2 << 12) | (2 << 14));  /* AF mode */
    GPIOA_AFRL  &= ~((0xF << 20) | (0xF << 24) | (0xF << 28));
    GPIOA_AFRL  |=  ((5 << 20) | (5 << 24) | (5 << 28));  /* AF5 = SPI1 */

    /* PA4 = CS (GPIO output, push-pull, start high) */
    GPIOA_MODER &= ~(3 << 8);
    GPIOA_MODER |=  (1 << 8);  /* output */
    CS_HIGH();

    /* SPI1 config: master, 8-bit, CPOL=0, CPHA=0, prescaler /4 (4MHz @ 16MHz) */
    SPI_CR1 = (1 << 2)   /* MSTR: master mode */
            | (1 << 3)   /* BR[2:0] = 001: fPCLK/4 */
            | (0 << 1)   /* CPOL = 0 */
            | (0 << 0)   /* CPHA = 0 */
            | (1 << 8)   /* SSI = 1 (internal slave select) */
            | (1 << 9);  /* SSM = 1 (software slave management) */
    SPI_CR1 |= (1 << 6); /* SPE: enable SPI */
}

static uint8_t spi_transfer(uint8_t tx)
{
    while (!(SPI_SR & (1 << 1)));  /* wait TXE */
    SPI_DR = tx;
    while (!(SPI_SR & (1 << 0)));  /* wait RXNE */
    return (uint8_t)SPI_DR;
}

/* --- W25Q128 operations --- */

static void w25q_write_enable(void)
{
    CS_LOW();
    spi_transfer(CMD_WRITE_ENABLE);
    CS_HIGH();
}

static void w25q_wait_busy(void)
{
    CS_LOW();
    spi_transfer(CMD_READ_STATUS);
    while (spi_transfer(0xFF) & STATUS_BUSY)
        ;
    CS_HIGH();
}

static void w25q_send_addr(uint32_t addr)
{
    spi_transfer((addr >> 16) & 0xFF);
    spi_transfer((addr >> 8) & 0xFF);
    spi_transfer(addr & 0xFF);
}

/* --- Flash driver API implementation --- */

static int flash_nor_init(const struct device *dev)
{
    (void)dev;
    spi_init();

    /* Verify JEDEC ID */
    CS_LOW();
    spi_transfer(CMD_JEDEC_ID);
    uint8_t mfr = spi_transfer(0xFF);
    uint8_t type = spi_transfer(0xFF);
    uint8_t cap = spi_transfer(0xFF);
    CS_HIGH();

    /* W25Q128: manufacturer=0xEF, type=0x40, capacity=0x18 */
    if (mfr != 0xEF || type != 0x40 || cap != 0x18)
        return -1;  /* wrong chip or not connected */

    return 0;
}

static int flash_nor_read(const struct device *dev, uint32_t offset,
                          void *buf, size_t len)
{
    (void)dev;
    uint8_t *d = buf;

    CS_LOW();
    spi_transfer(CMD_READ_DATA);
    w25q_send_addr(offset);
    for (size_t i = 0; i < len; i++)
        d[i] = spi_transfer(0xFF);
    CS_HIGH();

    return 0;
}

static int flash_nor_write(const struct device *dev, uint32_t offset,
                           const void *buf, size_t len)
{
    (void)dev;
    const uint8_t *s = buf;

    /* Page program: must not cross 256-byte page boundary */
    while (len > 0) {
        /* Bytes remaining in current page */
        size_t page_remain = 256 - (offset & 0xFF);
        size_t chunk = len < page_remain ? len : page_remain;

        w25q_write_enable();

        CS_LOW();
        spi_transfer(CMD_PAGE_PROGRAM);
        w25q_send_addr(offset);
        for (size_t i = 0; i < chunk; i++)
            spi_transfer(s[i]);
        CS_HIGH();

        w25q_wait_busy();  /* ~1-3ms per page */

        offset += chunk;
        s += chunk;
        len -= chunk;
    }

    return 0;
}

static int flash_nor_erase(const struct device *dev, uint32_t offset,
                           size_t size)
{
    (void)dev;

    /* Erase in 4KB sector increments */
    while (size > 0) {
        w25q_write_enable();

        CS_LOW();
        spi_transfer(CMD_SECTOR_ERASE);
        w25q_send_addr(offset);
        CS_HIGH();

        w25q_wait_busy();  /* ~50-400ms per sector */

        offset += 4096;
        size = size > 4096 ? size - 4096 : 0;
    }

    return 0;
}

static const struct flash_driver_api flash_nor_api = {
    .read = flash_nor_read,
    .write = flash_nor_write,
    .erase = flash_nor_erase,
};

/* ---- DT_INST instantiation ---- */

#define _FLASH_INST_LABEL(n) DT_INST_JEDEC_SPI_NOR_##n##_LABEL

#define JEDEC_SPI_NOR_DEFINE(n)                                     \
    static const struct flash_nor_config flash_cfg_##n = {          \
        .size = DT_INST_JEDEC_SPI_NOR_##n##_PROP_SIZE / 8,         \
        .page_size = DT_INST_JEDEC_SPI_NOR_##n##_PROP_PAGE_SIZE,   \
        .sector_size = DT_INST_JEDEC_SPI_NOR_##n##_PROP_SECTOR_SIZE, \
    };                                                              \
    DEVICE_DT_DEFINE(_FLASH_INST_LABEL(n),                          \
                     flash_nor_init, NULL, &flash_cfg_##n,           \
                     &flash_nor_api);

DT_INST_FOREACH_STATUS_OKAY(JEDEC_SPI_NOR, JEDEC_SPI_NOR_DEFINE)
