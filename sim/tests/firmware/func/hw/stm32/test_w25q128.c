/*
 * test_w25q128.c — W25Q128 flash device model tests via SPI3.
 * Tests JEDEC ID, read, page program, and sector erase.
 */
#include "test.h"

/* SPI3 registers at 0x40003C00 */
#define SPI3_CR1  (*(volatile unsigned int *)0x40003C00)
#define SPI3_CR2  (*(volatile unsigned int *)0x40003C04)
#define SPI3_SR   (*(volatile unsigned int *)0x40003C08)
#define SPI3_DR   (*(volatile unsigned int *)0x40003C0C)

/* GPIOB for CS (PB0) */
#define GPIOB_ODR (*(volatile unsigned int *)0x40020414)

#define SPI_CR1_SPE   (1 << 6)
#define SPI_CR1_MSTR  (1 << 2)
#define SPI_CR1_BR2   (1 << 5)  /* baud /8 */
#define SPI_SR_TXE    (1 << 1)
#define SPI_SR_RXNE   (1 << 0)

static void cs_low(void)  { GPIOB_ODR &= ~1; }
static void cs_high(void) { GPIOB_ODR |= 1; }

static void spi_init(void)
{
    GPIOB_ODR |= 1;  /* CS high (deasserted) */
    SPI3_CR1 = SPI_CR1_MSTR | SPI_CR1_BR2 | SPI_CR1_SPE;
}

static unsigned char spi_xfer(unsigned char byte)
{
    while (!(SPI3_SR & SPI_SR_TXE));
    SPI3_DR = byte;
    while (!(SPI3_SR & SPI_SR_RXNE));
    return (unsigned char)SPI3_DR;
}

void test_main(void)
{
    cycle_counter_init();
    spi_init();

    /* Test 1: JEDEC ID */
    TEST("jedec_id");
    cs_low();
    spi_xfer(0x9F);  /* command byte */
    unsigned char mfr = spi_xfer(0xFF);
    unsigned char typ = spi_xfer(0xFF);
    unsigned char cap = spi_xfer(0xFF);
    cs_high();
    CHECK(mfr == 0xEF);
    CHECK(typ == 0x40);
    CHECK(cap == 0x18);

    /* Test 2: Read data (flash starts as all 0xFF) */
    TEST("read_erased");
    cs_low();
    spi_xfer(0x03);  /* Read Data command */
    spi_xfer(0x00);  /* addr[23:16] */
    spi_xfer(0x10);  /* addr[15:8] = 0x1000 */
    spi_xfer(0x00);  /* addr[7:0] */
    unsigned char d0 = spi_xfer(0xFF);
    unsigned char d1 = spi_xfer(0xFF);
    cs_high();
    CHECK(d0 == 0xFF);
    CHECK(d1 == 0xFF);

    /* Test 3: Write Enable + Page Program + Read back */
    TEST("page_program");
    /* Write Enable */
    cs_low();
    spi_xfer(0x06);
    cs_high();

    /* Check WEL bit set */
    unsigned char st;
    cs_low();
    spi_xfer(0x05);  /* Read Status */
    st = spi_xfer(0xFF);
    cs_high();
    CHECK(st & 0x02);  /* WEL set */

    /* Page Program at address 0x002000 */
    cs_low();
    spi_xfer(0x02);
    spi_xfer(0x00);  /* addr[23:16] */
    spi_xfer(0x20);  /* addr[15:8] */
    spi_xfer(0x00);  /* addr[7:0] */
    spi_xfer(0xDE);
    spi_xfer(0xAD);
    spi_xfer(0xBE);
    spi_xfer(0xEF);
    cs_high();  /* commits page program */

    /* Wait for busy to clear */
    do {
        cs_low();
        spi_xfer(0x05);
        st = spi_xfer(0xFF);
        cs_high();
    } while (st & 0x01);

    /* Read back */
    cs_low();
    spi_xfer(0x03);
    spi_xfer(0x00);
    spi_xfer(0x20);
    spi_xfer(0x00);
    d0 = spi_xfer(0xFF);
    d1 = spi_xfer(0xFF);
    unsigned char d2 = spi_xfer(0xFF);
    unsigned char d3 = spi_xfer(0xFF);
    cs_high();
    CHECK(d0 == 0xDE);
    CHECK(d1 == 0xAD);
    CHECK(d2 == 0xBE);
    CHECK(d3 == 0xEF);

    /* Test 4: Sector Erase */
    TEST("sector_erase");
    cs_low();
    spi_xfer(0x06);  /* Write Enable */
    cs_high();
    cs_low();
    spi_xfer(0x20);  /* Sector Erase */
    spi_xfer(0x00);
    spi_xfer(0x20);  /* sector at 0x2000 */
    spi_xfer(0x00);
    cs_high();

    /* Wait for busy */
    do {
        cs_low();
        spi_xfer(0x05);
        st = spi_xfer(0xFF);
        cs_high();
    } while (st & 0x01);

    /* Read back — should be 0xFF */
    cs_low();
    spi_xfer(0x03);
    spi_xfer(0x00);
    spi_xfer(0x20);
    spi_xfer(0x00);
    d0 = spi_xfer(0xFF);
    d1 = spi_xfer(0xFF);
    cs_high();
    CHECK(d0 == 0xFF);
    CHECK(d1 == 0xFF);

    /* Test 5: Page boundary wrap */
    TEST("page_wrap");
    cs_low();
    spi_xfer(0x06);  /* Write Enable */
    cs_high();
    cs_low();
    spi_xfer(0x02);  /* Page Program */
    spi_xfer(0x00);
    spi_xfer(0x30);  /* addr = 0x30FE (2 bytes before page boundary) */
    spi_xfer(0xFE);
    spi_xfer(0xAA);  /* byte at 0x30FE */
    spi_xfer(0xBB);  /* byte at 0x30FF */
    spi_xfer(0xCC);  /* wraps to 0x3000 (not 0x3100) */
    spi_xfer(0xDD);  /* wraps to 0x3001 */
    cs_high();

    /* Wait busy */
    do {
        cs_low();
        spi_xfer(0x05);
        st = spi_xfer(0xFF);
        cs_high();
    } while (st & 0x01);

    /* Read 0x30FE — should be 0xAA */
    cs_low();
    spi_xfer(0x03);
    spi_xfer(0x00);
    spi_xfer(0x30);
    spi_xfer(0xFE);
    d0 = spi_xfer(0xFF);  /* 0x30FE = 0xAA */
    d1 = spi_xfer(0xFF);  /* 0x30FF = 0xBB */
    cs_high();
    CHECK(d0 == 0xAA);
    CHECK(d1 == 0xBB);

    /* Read 0x3000 — should be 0xCC (wrapped) */
    cs_low();
    spi_xfer(0x03);
    spi_xfer(0x00);
    spi_xfer(0x30);
    spi_xfer(0x00);
    d0 = spi_xfer(0xFF);  /* 0x3000 = 0xCC */
    d1 = spi_xfer(0xFF);  /* 0x3001 = 0xDD */
    cs_high();
    CHECK(d0 == 0xCC);
    CHECK(d1 == 0xDD);

    /* Read 0x3100 — should still be 0xFF (not written) */
    cs_low();
    spi_xfer(0x03);
    spi_xfer(0x00);
    spi_xfer(0x31);
    spi_xfer(0x00);
    d0 = spi_xfer(0xFF);
    cs_high();
    CHECK(d0 == 0xFF);

    TEST_DONE("w25q128");
}
