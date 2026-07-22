/*
 * gameboy_v2_test.h — Helpers for gameboy-v2 machine tests
 *
 * These tests run as firmware on the MCU with --machine gameboy-v2.
 * They use SPI1 to talk to the FPGA and check results via the
 * ILI9341 display chardev or UART assertions.
 */
#ifndef GAMEBOY_V2_TEST_H
#define GAMEBOY_V2_TEST_H

#include "test.h"

/* SPI1 registers (for PPU communication) */
#define SPI1_BASE  0x40013000
#define SPI1_CR1   (*(volatile unsigned int *)(SPI1_BASE + 0x00))
#define SPI1_CR2   (*(volatile unsigned int *)(SPI1_BASE + 0x04))
#define SPI1_SR    (*(volatile unsigned int *)(SPI1_BASE + 0x08))
#define SPI1_DR    (*(volatile unsigned int *)(SPI1_BASE + 0x0C))

/* GPIO for CS (PA4) */
#define GPIOA_ODR  (*(volatile unsigned int *)0x40020014)
#define GPIOA_BSRR (*(volatile unsigned int *)0x40020018)
#define GPIOB_IDR  (*(volatile unsigned int *)0x40020410)

/* CS control */
#define PPU_CS_LOW()   GPIOA_BSRR = (1 << (4 + 16))  /* reset PA4 */
#define PPU_CS_HIGH()  GPIOA_BSRR = (1 << 4)          /* set PA4 */

/* SPI send byte (blocking) */
static inline void ppu_spi_byte(unsigned char b) {
    SPI1_DR = b;
    while (!(SPI1_SR & 2)) {}  /* wait TXE */
}

/* Wait for SPI to finish (BSY clear) */
static inline void ppu_spi_wait(void) {
    while (SPI1_SR & 0x80) {}
}

/* PPU commands */
#define CMD_SPRITE_UPDATE  0x01
#define CMD_PIXEL_UPLOAD   0x02
#define CMD_BG_COLOR       0x03
#define CMD_TILE_TABLE     0x04

static inline void ppu_set_bg_color(unsigned short rgb565) {
    PPU_CS_LOW();
    ppu_spi_byte(CMD_BG_COLOR);
    ppu_spi_byte(rgb565 >> 8);
    ppu_spi_byte(rgb565 & 0xFF);
    ppu_spi_wait();
    PPU_CS_HIGH();
}

static inline void ppu_send_sprite_frame(unsigned char num,
    unsigned char x_lo, unsigned char x_hi, unsigned char y,
    unsigned char tile, unsigned char flags) {
    PPU_CS_LOW();
    ppu_spi_byte(CMD_SPRITE_UPDATE);
    ppu_spi_byte(num);
    if (num > 0) {
        ppu_spi_byte(x_lo);
        ppu_spi_byte(x_hi);
        ppu_spi_byte(y);
        ppu_spi_byte(tile);
        ppu_spi_byte(flags);
    }
    ppu_spi_wait();
    PPU_CS_HIGH();
}

/* Delay (busy wait) */
static inline void delay(volatile int n) { while (n-- > 0) {} }

#endif
