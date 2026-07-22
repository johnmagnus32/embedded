/*
 * test_fpga_passthrough.c - FPGA passthrough (wire) test
 * Netlist: passthrough.json (pin_in -> pin_out)
 * Wiring: GPIOA[0] -> FPGA pin_in, FPGA pin_out -> GPIOB[0]
 */
#include "test.h"
#include "fpga_test.h"

#define GPIOA_ODR (*(volatile unsigned int *)0x40020014)
#define GPIOB_IDR (*(volatile unsigned int *)0x40020410)

void test_main(void)
{
    TEST("passthrough_low");
    GPIOA_ODR = 0;
    delay(100);
    CHECK((GPIOB_IDR & 1) == 0);

    TEST("passthrough_high");
    GPIOA_ODR = 1;
    delay(100);
    CHECK((GPIOB_IDR & 1) == 1);

    TEST("passthrough_toggle");
    GPIOA_ODR = 0;
    delay(100);
    CHECK((GPIOB_IDR & 1) == 0);

    TEST_DONE("fpga_passthrough");
}
