/*
 * fpga_test.h - Shared helpers for FPGA device tests.
 *
 * The FPGA runs at 48MHz, MCU at 16MHz (3 FPGA ticks per MCU tick).
 * delay(n) burns n MCU cycles, giving the FPGA ~3n clock edges to settle.
 */
#ifndef FPGA_TEST_H
#define FPGA_TEST_H

static inline void delay(volatile unsigned int n)
{
    while (n--) __asm volatile("nop");
}

/* Enough delay for combinational logic to settle (1 FPGA tick) */
#define SETTLE()  delay(10)

/* Enough delay for N pipeline stages (registered inputs/outputs) */
#define PIPELINE(n) delay(10 * (n))

#endif
