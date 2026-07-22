/*
 * test_dma_i2s_kick.c — Regression: DMA request_pending survives CR=0 reset
 *
 * Bug 1: Writing SxCR=0 (disable) cleared request_pending even when the
 *         stream was never enabled. Firmware DMA drivers write CR=0 before
 *         configuring, which wiped the pending request from kick_dma.
 *
 * Bug 2: Enabling a stream that already had request_pending didn't set
 *         any_pending, so stm32_dma_tick never processed the first transfer.
 *
 * Sequence (matches real I2S DMA startup):
 *   1. SPI2 I2S mode + TXDMAEN → kick_dma sets request_pending on stream 4
 *   2. DMA1 S4CR = 0  (firmware "disable first" pattern)
 *   3. Configure S4 PAR/M0AR/NDTR
 *   4. S4CR = ... | EN
 *   5. Verify DMA transfers data from RAM to SPI2 DR
 */
#define TEST_CUSTOM_VECTORS
#include "test.h"

/* SPI2 at 0x40003800 */
#define SPI2_CR1     (*(volatile unsigned int *)0x40003800)
#define SPI2_CR2     (*(volatile unsigned int *)0x40003804)
#define SPI2_SR      (*(volatile unsigned int *)0x40003808)
#define SPI2_DR      (*(volatile unsigned int *)0x4000380C)
#define SPI2_I2SCFGR (*(volatile unsigned int *)0x4000381C)
#define SPI2_I2SPR   (*(volatile unsigned int *)0x40003820)

/* DMA1 Stream 4: base + 0x10 + 4*0x18 = 0x70 */
#define DMA1_HISR    (*(volatile unsigned int *)0x40026004)
#define DMA1_HIFCR   (*(volatile unsigned int *)0x4002600C)
#define DMA1_S4CR    (*(volatile unsigned int *)0x40026070)
#define DMA1_S4NDTR  (*(volatile unsigned int *)0x40026074)
#define DMA1_S4PAR   (*(volatile unsigned int *)0x40026078)
#define DMA1_S4M0AR  (*(volatile unsigned int *)0x4002607C)

#define CR_EN    (1 << 0)
#define CR_TCIE  (1 << 4)
#define CR_HTIE  (1 << 5)
#define CR_DIR_M2P (1 << 6)
#define CR_CIRC  (1 << 8)
#define CR_MINC  (1 << 10)
#define CR_PSIZE_HW (1 << 11)  /* 16-bit */
#define CR_MSIZE_HW (1 << 13)

#define NVIC_ISER0 (*(volatile unsigned int *)0xE000E100)

static volatile int dma_tc_count;

void systick_handler(void) {}
void pendsv_handler(void) {}
void svc_handler(void) {}
void memmanage_handler(void) { while(1); }

/* DMA1 Stream 4 = IRQ 15 */
void dma1_stream4_handler(void)
{
    dma_tc_count++;
    DMA1_HIFCR = (1 << 5);  /* clear TCIF4 */
}

static unsigned short audio_buf[8] = {
    1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000
};

extern unsigned int _stack_top;
void test_main(void);

void __attribute__((naked)) reset_handler(void)
{
    __asm volatile("bl test_main\n b .\n");
}

__attribute__((section(".vector_table"), used))
void (* const vectors[])(void) = {
    (void (*)(void))&_stack_top,
    reset_handler, 0, 0,
    memmanage_handler, 0, 0, 0, 0, 0, 0,
    svc_handler, 0, 0,
    pendsv_handler,
    systick_handler,
    /* IRQ 0-14 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    dma1_stream4_handler,  /* IRQ 15 */
};

void test_main(void)
{
    TEST("dma_i2s_kick");

    /* Enable DMA1 Stream 4 IRQ (IRQ 15) */
    NVIC_ISER0 = (1 << 15);

    /* Step 1: Put SPI2 in I2S mode, set TXDMAEN.
     * This triggers kick_dma → sets request_pending on DMA1 stream 4. */
    SPI2_I2SCFGR = (1 << 11) | (1 << 10);  /* I2SMOD + I2SE */
    SPI2_CR2 = (1 << 1);  /* TXDMAEN */

    /* Step 2: Write CR=0 ("disable first" — the bug cleared request_pending here) */
    DMA1_S4CR = 0;

    /* Step 3: Configure stream */
    DMA1_S4PAR  = (unsigned int)&SPI2_DR;
    DMA1_S4M0AR = (unsigned int)audio_buf;
    DMA1_S4NDTR = 8;

    /* Step 4: Enable with mem-to-periph, 16-bit, MINC, TCIE */
    dma_tc_count = 0;
    DMA1_S4CR = CR_DIR_M2P | CR_MINC | CR_PSIZE_HW | CR_MSIZE_HW | CR_TCIE | CR_EN;

    /* Wait for transfer complete — spin with timeout */
    for (volatile int i = 0; i < 100000 && dma_tc_count == 0; i++) {}

    /* Verify DMA transferred all 8 half-words (4 stereo samples) */
    CHECK(dma_tc_count >= 1);
    CHECK(DMA1_S4NDTR == 0);

    TEST_DONE("dma_i2s_kick");
}
