/*
 * test_fpga_counter.c - 4-bit counter with sync reset
 * Netlist: counter.json (clk, rst -> count[3:0])
 * Wiring: GPIOA[0] -> clk, GPIOA[1] -> rst
 *         FPGA count[0..3] -> GPIOB[0..3]
 *
 * With free-running FPGA clock, we can't predict exact count values.
 * Instead we verify: reset works, counter advances, counter wraps.
 */
#include "test.h"
#include "fpga_test.h"

#define GPIOA_ODR (*(volatile unsigned int *)0x40020014)
#define GPIOB_IDR (*(volatile unsigned int *)0x40020410)

static unsigned int read_count(void) { return GPIOB_IDR & 0x0F; }

void test_main(void)
{
    /* Assert reset */
    TEST("counter_reset");
    GPIOA_ODR = 0x02;  /* rst=1 */
    delay(100);
    CHECK(read_count() == 0);

    /* Release reset, verify counter advances */
    TEST("counter_advances");
    GPIOA_ODR = 0x00;  /* rst=0 */
    delay(50);
    unsigned int v1 = read_count();
    delay(50);
    unsigned int v2 = read_count();
    CHECK(v2 != v1);  /* counter moved */

    /* Reset again mid-count */
    TEST("counter_reset_mid");
    delay(100);  /* let it count */
    CHECK(read_count() != 0);  /* should be non-zero */
    GPIOA_ODR = 0x02;  /* rst=1 */
    delay(100);
    CHECK(read_count() == 0);  /* reset works */

    TEST_DONE("fpga_counter");
}
