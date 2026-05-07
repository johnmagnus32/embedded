/*
 * test_dma_spi.c — Minimal DMA-to-SPI transfer test.
 * Transfers 4 bytes via DMA2 Stream 3 to SPI1 DR.
 */
#define TEST_CUSTOM_VECTORS
#include "test.h"

#define SPI1_CR1 (*(volatile unsigned int *)0x40013000)
#define SPI1_CR2 (*(volatile unsigned int *)0x40013004)
#define SPI1_SR  (*(volatile unsigned int *)0x40013008)

/* DMA2 Stream 3 */
#define DMA2_LISR    (*(volatile unsigned int *)0x40026400)
#define DMA2_LIFCR   (*(volatile unsigned int *)0x40026408)
#define DMA2_S3CR    (*(volatile unsigned int *)0x40026458)
#define DMA2_S3NDTR  (*(volatile unsigned int *)0x4002645C)
#define DMA2_S3PAR   (*(volatile unsigned int *)0x40026460)
#define DMA2_S3M0AR  (*(volatile unsigned int *)0x40026464)

#define CR_EN      (1 << 0)
#define CR_TCIE    (1 << 4)
#define CR_DIR_M2P (1 << 6)
#define CR_MINC    (1 << 10)
#define DMA2_S3_TCIF (1 << 27)

static unsigned char buf[4] = {0xAA, 0xBB, 0xCC, 0xDD};

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
    cycle_counter_init();

    /* Init buf at runtime (no .data init) */
    buf[0] = 0xAA; buf[1] = 0xBB; buf[2] = 0xCC; buf[3] = 0xDD;

    /* Enable SPI1: master, /2 prescaler */
    SPI1_CR1 = (1 << 6) | (1 << 2);

    /* Verify SPI works with CPU polling first */
    TEST("cpu_spi_sanity");
    while (!(SPI1_SR & (1 << 1))) {}
    *((volatile unsigned int *)0x4001300C) = 0x42;
    /* Wait for TXE to come back */
    while (!(SPI1_SR & (1 << 1))) {}
    CHECK(1);  /* if we get here, CPU->SPI works */

    semi_puts("cpu spi ok\n");

    /* Now try DMA */
    TEST("dma_spi_4bytes");

    /* Configure DMA2 Stream 3 */
    DMA2_S3CR   = 0;  /* disable */
    DMA2_S3PAR  = 0x4001300C;  /* SPI1_DR */
    DMA2_S3M0AR = (unsigned int)buf;
    DMA2_S3NDTR = 4;
    DMA2_S3CR   = CR_DIR_M2P | CR_MINC | CR_TCIE;

    /* Clear TC flag */
    DMA2_LIFCR = DMA2_S3_TCIF;

    semi_puts("dma configured\n");

    /* Enable DMA stream */
    DMA2_S3CR |= CR_EN;

    semi_puts("dma enabled\n");

    /* Enable SPI TXDMAEN — kicks first request */
    SPI1_CR2 = (1 << 1);

    semi_puts("txdmaen set\n");

    /* Wait for transfer complete with timeout */
    unsigned int start = cycles();
    int done = 0;
    while (cycles() - start < 1000000) {
        if (DMA2_LISR & DMA2_S3_TCIF) {
            done = 1;
            break;
        }
    }

    unsigned int elapsed = cycles() - start;
    semi_puts("elapsed: "); semi_putdec(elapsed); semi_puts(" cycles\n");
    semi_puts("ndtr: "); semi_putdec(DMA2_S3NDTR); semi_puts("\n");
    semi_puts("hisr: "); semi_puthex(DMA2_LISR); semi_puts("\n");

    SPI1_CR2 = 0;
    DMA2_LIFCR = DMA2_S3_TCIF;

    CHECK(done);

    TEST_DONE("dma_spi");
}
