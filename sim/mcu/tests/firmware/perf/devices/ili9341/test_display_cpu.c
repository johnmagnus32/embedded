/*
 * test_perf_chardev_cpu.c — Chardev FPS while CPU polls SPI.
 *
 * Firmware continuously writes partial frames via CPU polling.
 * Test runner measures frames arriving on the display chardev.
 * Verifies the ILI9341 60Hz refresh keeps running during SPI activity.
 */
#define TEST_CUSTOM_VECTORS
#include "test.h"

#define SPI1_CR1 (*(volatile unsigned int *)0x40013000)
#define SPI1_SR  (*(volatile unsigned int *)0x40013008)
#define SPI1_DR  (*(volatile unsigned int *)0x4001300C)
#define GPIOA_BSRR (*(volatile unsigned int *)0x40020018)
#define DC_PIN 3

static inline void dc_command(void) { GPIOA_BSRR = (1 << (DC_PIN + 16)); }
static inline void dc_data(void)    { GPIOA_BSRR = (1 << DC_PIN); }
static inline void spi_write(unsigned int b)
{
    while (!(SPI1_SR & (1 << 1))) {}
    SPI1_DR = b;
}
static void ili_cmd(unsigned int cmd) { dc_command(); spi_write(cmd); }

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

    /* Continuously write partial frames until killed */
    while (1) {
        ili_cmd(0x2A); dc_data();
        spi_write(0); spi_write(0); spi_write(0); spi_write(99);
        ili_cmd(0x2B); dc_data();
        spi_write(0); spi_write(0); spi_write(0); spi_write(99);
        ili_cmd(0x2C); dc_data();
        for (int i = 0; i < 100 * 100; i++) {
            spi_write(0xFF);
            spi_write(0x00);
        }
    }
}
