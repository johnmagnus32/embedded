/*
 * test_trace.c — Firmware that produces known SPI and I2S output for trace validation.
 *
 * SPI1: Sends a known 8-byte sequence (display commands)
 * SPI3: Sends JEDEC ID read (0x9F) + 4-byte read at address 0
 * I2S2: Outputs 8 known 16-bit samples
 *
 * Runs on the gameboy machine. Exits via semihosting after all output is done.
 */
#include "test.h"

/* SPI1 registers (ILI9341 display) */
#define SPI1_CR1  (*(volatile unsigned int *)0x40013000)
#define SPI1_SR   (*(volatile unsigned int *)0x40013008)
#define SPI1_DR   (*(volatile unsigned int *)0x4001300C)

/* SPI3 registers (W25Q128 flash) */
#define SPI3_CR1  (*(volatile unsigned int *)0x40003C00)
#define SPI3_SR   (*(volatile unsigned int *)0x40003C08)
#define SPI3_DR   (*(volatile unsigned int *)0x40003C0C)

/* GPIOB ODR for CS pins */
#define GPIOB_ODR (*(volatile unsigned int *)0x40020414)

/* I2S2/SPI2 registers */
#define SPI2_CR1  (*(volatile unsigned int *)0x40003800)
#define SPI2_CR2  (*(volatile unsigned int *)0x40003804)
#define SPI2_SR   (*(volatile unsigned int *)0x40003808)
#define SPI2_DR   (*(volatile unsigned int *)0x4000380C)
#define SPI2_I2SCFGR (*(volatile unsigned int *)0x4000381C)
#define SPI2_I2SPR   (*(volatile unsigned int *)0x40003820)

#define SPI_CR1_SPE   (1 << 6)
#define SPI_CR1_MSTR  (1 << 2)
#define SPI_CR1_BR2   (1 << 5)
#define SPI_SR_TXE    (1 << 1)
#define SPI_SR_RXNE   (1 << 0)

static void spi1_init(void)
{
    SPI1_CR1 = SPI_CR1_MSTR | SPI_CR1_BR2 | SPI_CR1_SPE;
}

static void spi3_init(void)
{
    GPIOB_ODR |= 1;  /* CS high */
    SPI3_CR1 = SPI_CR1_MSTR | SPI_CR1_BR2 | SPI_CR1_SPE;
}

static unsigned char spi1_xfer(unsigned char byte)
{
    while (!(SPI1_SR & SPI_SR_TXE));
    SPI1_DR = byte;
    while (!(SPI1_SR & SPI_SR_RXNE));
    return (unsigned char)SPI1_DR;
}

static unsigned char spi3_xfer(unsigned char byte)
{
    while (!(SPI3_SR & SPI_SR_TXE));
    SPI3_DR = byte;
    while (!(SPI3_SR & SPI_SR_RXNE));
    return (unsigned char)SPI3_DR;
}

static void cs3_low(void)  { GPIOB_ODR &= ~1; }
static void cs3_high(void) { GPIOB_ODR |= 1; }

static void i2s_init(void)
{
    /* Configure SPI2 as I2S master transmit */
    SPI2_I2SCFGR = (1 << 11) | (1 << 10);  /* I2SMOD=1, I2SCFG=master TX */
    SPI2_I2SPR = 4;  /* prescaler */
    SPI2_I2SCFGR |= (1 << 10);  /* I2SE=1 (enable) - already set above */
}

static void i2s_send(unsigned short sample)
{
    while (!(SPI2_SR & SPI_SR_TXE));
    SPI2_DR = sample;
}

void test_main(void)
{
    spi1_init();
    spi3_init();
    i2s_init();

    /* --- SPI1: send known 8-byte sequence --- */
    spi1_xfer(0x2A);  /* Column Address Set */
    spi1_xfer(0x00);
    spi1_xfer(0x00);
    spi1_xfer(0x01);
    spi1_xfer(0x3F);
    spi1_xfer(0x2C);  /* Memory Write */
    spi1_xfer(0xF8);  /* pixel high byte (red) */
    spi1_xfer(0x00);  /* pixel low byte */

    /* --- SPI3: JEDEC ID + read 4 bytes from address 0 --- */
    cs3_low();
    spi3_xfer(0x9F);
    spi3_xfer(0xFF);
    spi3_xfer(0xFF);
    spi3_xfer(0xFF);
    cs3_high();

    cs3_low();
    spi3_xfer(0x03);  /* Read Data */
    spi3_xfer(0x00);  /* addr[23:16] */
    spi3_xfer(0x00);  /* addr[15:8] */
    spi3_xfer(0x00);  /* addr[7:0] */
    spi3_xfer(0xFF);  /* data byte 0 */
    spi3_xfer(0xFF);  /* data byte 1 */
    spi3_xfer(0xFF);  /* data byte 2 */
    spi3_xfer(0xFF);  /* data byte 3 */
    cs3_high();

    /* --- I2S2: send 8 known samples --- */
    i2s_send(0x1234);
    i2s_send(0x5678);
    i2s_send(0x9ABC);
    i2s_send(0xDEF0);
    i2s_send(0x0000);
    i2s_send(0x7FFF);
    i2s_send(0x8000);
    i2s_send(0xFFFF);

    /* Small delay to let I2S drain */
    for (volatile int i = 0; i < 1000; i++);

    semi_exit(0);
}
