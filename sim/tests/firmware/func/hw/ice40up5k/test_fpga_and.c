/*
 * test_fpga_and.c - LUT4 as 2-input AND
 * Netlist: and_gate.json (a, b -> a&b -> y)
 * Wiring: GPIOA[0] -> a, GPIOA[1] -> b, FPGA y -> GPIOB[0]
 */
#include "test.h"
#include "fpga_test.h"

#define GPIOA_ODR (*(volatile unsigned int *)0x40020014)
#define GPIOB_IDR (*(volatile unsigned int *)0x40020410)

void test_main(void)
{
    TEST("and_00");
    GPIOA_ODR = 0x00;
    delay(100);
    CHECK((GPIOB_IDR & 1) == 0);

    TEST("and_10");
    GPIOA_ODR = 0x01;
    delay(100);
    CHECK((GPIOB_IDR & 1) == 0);

    TEST("and_01");
    GPIOA_ODR = 0x02;
    delay(100);
    CHECK((GPIOB_IDR & 1) == 0);

    TEST("and_11");
    GPIOA_ODR = 0x03;
    delay(100);
    CHECK((GPIOB_IDR & 1) == 1);

    TEST_DONE("fpga_and");
}
