/*
 * test_perf_chardev_dma.c — Chardev FPS while DMA drives SPI.
 *
 * Firmware continuously transfers partial frames via DMA to SPI1.
 * Test runner measures frames arriving on the display chardev.
 * Verifies the ILI9341 60Hz refresh keeps running during DMA activity.
 */
#define TEST_CUSTOM_VECTORS
#include "test.h"

#define SPI1_CR1 (*(volatile unsigned int *)0x40013000)
#define SPI1_CR2 (*(volatile unsigned int *)0x40013004)
#define SPI1_SR  (*(volatile unsigned int *)0x40013008)
#define SPI1_DR  (*(volatile unsigned int *)0x4001300C)
#define GPIOA_BSRR (*(volatile unsigned int *)0x40020018)

#define DMA2_LISR    (*(volatile unsigned int *)0x40026400)
#define DMA2_LIFCR   (*(volatile unsigned int *)0x40026408)
#define DMA2_S3CR    (*(volatile unsigned int *)0x40026458)
#define DMA2_S3NDTR  (*(volatile unsigned int *)0x4002645C)
#define DMA2_S3PAR   (*(volatile unsigned int *)0x40026460)
#define DMA2_S3M0AR  (*(volatile unsigned int *)0x40026464)

#define DC_PIN 3
#define CR_EN      (1 << 0)
#define CR_TCIE    (1 << 4)
#define CR_DIR_M2P (1 << 6)
#define CR_MINC    (1 << 10)
#define DMA2_S3_TCIF (1 << 27)

#define RECT_PIXELS 10000
static unsigned char pixel_buf[RECT_PIXELS * 2];

static inline void dc_command(void) { GPIOA_BSRR = (1 << (DC_PIN + 16)); }
static inline void dc_data(void)    { GPIOA_BSRR = (1 << DC_PIN); }
static inline void spi_write_byte(unsigned int b)
{
    while (!(SPI1_SR & (1 << 1))) {}
    SPI1_DR = b;
}
static void ili_cmd(unsigned int cmd) { dc_command(); spi_write_byte(cmd); }

static void dma_transfer(int nbytes)
{
    DMA2_S3CR   = 0;
    DMA2_S3PAR  = (unsigned int)&SPI1_DR;
    DMA2_S3M0AR = (unsigned int)pixel_buf;
    DMA2_S3NDTR = nbytes;
    DMA2_S3CR   = CR_DIR_M2P | CR_MINC | CR_TCIE;
    DMA2_LIFCR  = DMA2_S3_TCIF;
    DMA2_S3CR  |= CR_EN;
    SPI1_CR2 = (1 << 1);
    while (!(DMA2_LISR & DMA2_S3_TCIF)) {}
    SPI1_CR2 = 0;
    DMA2_LIFCR = DMA2_S3_TCIF;
}

void systick_handler(void) {}
void pendsv_handler(void) {}
void svc_handler(void) {}
void memmanage_handler(void) { while(1); }

extern unsigned int _stack_top;
void test_main(void);
void __attribute__((naked)) reset_handler(void) { __asm volatile("bl test_main\n b .\n"); }

__attribute__((section(".vector_table"), used))
void (* const vectors[])(void) = {
    (void (*)(void))&_stack_top, reset_handler,
    0, 0, memmanage_handler, 0, 0, 0, 0, 0, 0,
    svc_handler, 0, 0, pendsv_handler, systick_handler,
};

void test_main(void)
{
    SPI1_CR1 = (1 << 6) | (1 << 2);
    ili_cmd(0x11);
    ili_cmd(0x29);

    for (int i = 0; i < RECT_PIXELS * 2; i++)
        pixel_buf[i] = (unsigned char)(i & 0xFF);

    /* Continuously DMA partial frames until killed */
    while (1) {
        ili_cmd(0x2A); dc_data();
        spi_write_byte(0); spi_write_byte(0); spi_write_byte(0); spi_write_byte(99);
        ili_cmd(0x2B); dc_data();
        spi_write_byte(0); spi_write_byte(0); spi_write_byte(0); spi_write_byte(99);
        ili_cmd(0x2C); dc_data();
        dma_transfer(RECT_PIXELS * 2);
    }
}
