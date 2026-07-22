/*
 * test_fpga_inverter.c - LUT4 as NOT gate
 * Netlist: inverter.json (a -> ~a -> y)
 * Wiring: GPIOA[0] -> FPGA a, FPGA y -> GPIOB[0]
 */
#include "test.h"
#include "fpga_test.h"

#define GPIOA_ODR (*(volatile unsigned int *)0x40020014)
#define GPIOB_IDR (*(volatile unsigned int *)0x40020410)

void test_main(void)
{
    TEST("inv_0");
    GPIOA_ODR = 0;
    delay(100);
    CHECK((GPIOB_IDR & 1) == 1);

    TEST("inv_1");
    GPIOA_ODR = 1;
    delay(100);
    CHECK((GPIOB_IDR & 1) == 0);

    TEST_DONE("fpga_inverter");
}
