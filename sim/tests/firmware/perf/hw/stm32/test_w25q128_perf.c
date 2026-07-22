/*
 * test_w25q128_perf.c — W25Q128 flash read/write throughput via SPI3.
 *
 * Measures sequential read and page-program throughput through the
 * paced SPI model. At default /2 prescaler (16 cycles/byte):
 *   Read 4KB: 4096 bytes * 16 cycles = 65536 cycles + overhead
 *   Write 256B page: 256 bytes * 16 cycles = 4096 cycles + overhead
 */
#include "test.h"

#define SPI3_CR1  (*(volatile unsigned int *)0x40003C00)
#define SPI3_SR   (*(volatile unsigned int *)0x40003C08)
#define SPI3_DR   (*(volatile unsigned int *)0x40003C0C)
#define GPIOB_ODR (*(volatile unsigned int *)0x40020414)

#define SPI_CR1_SPE   (1 << 6)
#define SPI_CR1_MSTR  (1 << 2)
#define SPI_SR_TXE    (1 << 1)
#define SPI_SR_RXNE   (1 << 0)

static void cs_low(void)  { GPIOB_ODR &= ~1; }
static void cs_high(void) { GPIOB_ODR |= 1; }

static unsigned char spi_xfer(unsigned char byte)
{
    while (!(SPI3_SR & SPI_SR_TXE));
    SPI3_DR = byte;
    while (!(SPI3_SR & SPI_SR_RXNE));
    return (unsigned char)SPI3_DR;
}

static void flash_wait_busy(void)
{
    unsigned char st;
    do {
        cs_low();
        spi_xfer(0x05);
        st = spi_xfer(0xFF);
        cs_high();
    } while (st & 0x01);
}

void test_main(void)
{
    cycle_counter_init();
    GPIOB_ODR |= 1;
    SPI3_CR1 = SPI_CR1_MSTR | SPI_CR1_SPE;

    /* --- Read throughput: 4KB sequential read --- */
    TEST("flash_read_4k");
    unsigned int c0 = cycles();
    unsigned int t0 = semi_clock_us();

    cs_low();
    spi_xfer(0x0B);  /* Fast Read */
    spi_xfer(0x00);  /* addr[23:16] */
    spi_xfer(0x00);  /* addr[15:8] */
    spi_xfer(0x00);  /* addr[7:0] */
    spi_xfer(0xFF);  /* dummy byte */
    for (int i = 0; i < 4096; i++)
        spi_xfer(0xFF);
    cs_high();

    unsigned int c1 = cycles();
    unsigned int t1 = semi_clock_us();
    unsigned int read_us = t1 - t0;
    unsigned int read_cycles = c1 - c0;
    unsigned int read_cyc_per_byte = read_cycles / 4096;

    semi_puts("flash read 4KB: ");
    semi_putdec(read_us / 1000); semi_puts("ms, ");
    semi_putdec(read_cyc_per_byte); semi_puts(" cycles/byte\n");

    /* At 16 cycles/byte pacing + overhead, expect < 50 cycles/byte */
    CHECK_RANGE(read_cyc_per_byte, 10, 50);

    /* --- Write throughput: 256-byte page program --- */
    TEST("flash_write_page");

    /* Write enable */
    cs_low(); spi_xfer(0x06); cs_high();

    c0 = cycles();
    t0 = semi_clock_us();

    cs_low();
    spi_xfer(0x02);  /* Page Program */
    spi_xfer(0x01);  /* addr = 0x010000 */
    spi_xfer(0x00);
    spi_xfer(0x00);
    for (int i = 0; i < 256; i++)
        spi_xfer(i & 0xFF);
    cs_high();

    c1 = cycles();
    t1 = semi_clock_us();
    unsigned int write_us = t1 - t0;
    unsigned int write_cycles = c1 - c0;
    unsigned int write_cyc_per_byte = write_cycles / 256;

    semi_puts("flash write 256B: ");
    semi_putdec(write_us / 1000); semi_puts("ms, ");
    semi_putdec(write_cyc_per_byte); semi_puts(" cycles/byte\n");

    CHECK_RANGE(write_cyc_per_byte, 10, 50);

    /* Wait for program to complete, measure busy time */
    TEST("flash_busy_time");
    c0 = cycles();
    flash_wait_busy();
    c1 = cycles();
    unsigned int busy_cycles = c1 - c0;

    semi_puts("flash busy after write: ");
    semi_putdec(busy_cycles); semi_puts(" cycles\n");

    /* Busy should be ~11200 cycles (modeled page program time) */
    CHECK_RANGE(busy_cycles, 5000, 20000);

    /* Verify written data */
    TEST("flash_verify");
    cs_low();
    spi_xfer(0x03);  /* Read */
    spi_xfer(0x01);
    spi_xfer(0x00);
    spi_xfer(0x00);
    int errors = 0;
    for (int i = 0; i < 256; i++) {
        unsigned char got = spi_xfer(0xFF);
        if (got != (unsigned char)(i & 0xFF)) errors++;
    }
    cs_high();
    CHECK(errors == 0);

    TEST_DONE("w25q128_perf");
}
