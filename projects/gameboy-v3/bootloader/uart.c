/*
 * uart.c — UART0 driver for the gameboy-v3 bootloader (T113-S3, PE2/PE3).
 *
 * Every register value here was taken from the mainline sources for this exact
 * SoC (sun20i / T113-S3 / R528), not guessed:
 *   - CCU base, UART0 BGR reg/bits  : kernel drivers/clk/sunxi-ng/ccu-sun20i-d1.c
 *   - UART0 base                    : kernel DT serial@2500000 / U-Boot serial.h
 *   - PIO base, bank size, mux func : U-Boot include/sunxi_gpio.h + board.c (R528)
 */

#include <stdint.h>
#include "uart.h"

/* ---- register bases (sun20i / T113-S3) ----------------------------------- */
#define CCU_BASE        0x02001000u   /* clock control unit                   */
#define PIO_BASE        0x02000000u   /* GPIO / pin controller                */
#define UART0_BASE      0x02500000u   /* UART0 (16550-compatible)             */

/* CCU: bus gate + reset for the UARTs live in one register at 0x90c.
 * gate  = BIT(n) for UARTn ; reset = BIT(16+n).  (ccu-sun20i-d1.c)           */
#define CCU_UART_BGR    (CCU_BASE + 0x90c)
#define UART0_GATE      (1u << 0)
#define UART0_RESET     (1u << 16)

/* PIO: NCAT2 banks are 0x30 apart; bank E = index 4. Pins 0-7 share CFG0.    */
#define PIO_BANK_SIZE   0x30u
#define PE_BANK         (PIO_BASE + 4u * PIO_BANK_SIZE)   /* 0x020000C0        */
#define PE_CFG0         (PE_BANK + 0x00u)                 /* config for PE0-7  */
#define PE_MUX_UART0    6u            /* PE2/PE3 alt-function 6 = UART0        */

/* UART 16550 register offsets (reg-shift = 2 -> each reg 4 bytes apart) ----- */
#define UART_THR        0x00u
#define UART_DLL        0x00u
#define UART_DLH        0x04u
#define UART_IER        0x04u
#define UART_FCR        0x08u
#define UART_LCR        0x0cu
#define UART_LSR        0x14u
#define UART_LSR_THRE   (1u << 5)

/* BROM leaves the UART mod clock on the 24 MHz oscillator (no PLL yet).      */
#define UART_CLK_HZ     24000000u
#define BAUD            115200u
#define UART_DIVISOR    (UART_CLK_HZ / (16u * BAUD))   /* = 13                 */

static inline void mmio_w(uint32_t a, uint32_t v) { *(volatile uint32_t *)a = v; }
static inline uint32_t mmio_r(uint32_t a) { return *(volatile uint32_t *)a; }

void uart0_init(void)
{
	/* 1. Mux PE2/PE3 to UART0 (alt func 6): PE2 -> [11:8], PE3 -> [15:12]. */
	uint32_t cfg = mmio_r(PE_CFG0);
	cfg &= ~((0xfu << (2 * 4)) | (0xfu << (3 * 4)));
	cfg |=  ((PE_MUX_UART0 << (2 * 4)) | (PE_MUX_UART0 << (3 * 4)));
	mmio_w(PE_CFG0, cfg);

	/* 2. Ungate + de-reset the UART0 bus clock. */
	uint32_t bgr = mmio_r(CCU_UART_BGR);
	bgr |= UART0_GATE;
	mmio_w(CCU_UART_BGR, bgr);
	bgr |= UART0_RESET;
	mmio_w(CCU_UART_BGR, bgr);

	/* 3. Program the 16550: 8N1, FIFO on, baud divisor via DLAB. */
	mmio_w(UART0_BASE + UART_IER, 0x00);
	mmio_w(UART0_BASE + UART_FCR, 0x01);
	mmio_w(UART0_BASE + UART_LCR, 0x80);            /* DLAB=1 */
	mmio_w(UART0_BASE + UART_DLL, UART_DIVISOR & 0xff);
	mmio_w(UART0_BASE + UART_DLH, (UART_DIVISOR >> 8) & 0xff);
	mmio_w(UART0_BASE + UART_LCR, 0x03);            /* DLAB=0, 8N1 */
}

void uart0_putc(char c)
{
	while (!(mmio_r(UART0_BASE + UART_LSR) & UART_LSR_THRE))
		;
	mmio_w(UART0_BASE + UART_THR, (uint32_t)(uint8_t)c);
}

void uart0_puts(const char *s)
{
	for (; *s; s++) {
		if (*s == '\n')
			uart0_putc('\r');
		uart0_putc(*s);
	}
}
