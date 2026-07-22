/*
 * test_fpga_dff.c - D flip-flop test
 * Netlist: dff.json (clk, d -> DFF -> q)
 * Wiring: GPIOA[0] -> clk (driven by FPGA internally), GPIOA[1] -> d
 *         FPGA q -> GPIOB[0]
 *
 * With free-running FPGA clock, we can't test "hold" (Q doesn't change
 * without a clock edge) because edges happen continuously. Instead we
 * verify that Q eventually captures D.
 */
#include "test.h"
#include "fpga_test.h"

#define GPIOA_ODR (*(volatile unsigned int *)0x40020014)
#define GPIOB_IDR (*(volatile unsigned int *)0x40020410)

void test_main(void)
{
    TEST("dff_capture_1");
    GPIOA_ODR = 0x02;  /* d=1 */
    delay(100);
    CHECK((GPIOB_IDR & 1) == 1);

    TEST("dff_capture_0");
    GPIOA_ODR = 0x00;  /* d=0 */
    delay(100);
    CHECK((GPIOB_IDR & 1) == 0);

    TEST("dff_capture_1_again");
    GPIOA_ODR = 0x02;
    delay(100);
    CHECK((GPIOB_IDR & 1) == 1);

    TEST_DONE("fpga_dff");
}
