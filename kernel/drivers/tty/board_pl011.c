/*
 * board_pl011.c — ARM PL011 PrimeCell UART driver (QEMU `-M virt`).
 *
 * QEMU's virt board puts a PL011 at 0x09000000 and has it ready at reset (it
 * ignores the baud/clock divisors), so init is minimal. Register offsets are
 * from the ARM PL011 TRM (DDI0183):
 *   UARTDR   0x00  data (write=TX, read=RX)
 *   UARTFR   0x18  flags: bit4 RXFE (rx FIFO empty), bit5 TXFF (tx FIFO full)
 *   UARTIBRD 0x24 / UARTFBRD 0x28  baud divisors
 *   UARTLCRH 0x2C  line control: bits [6:5]=WLEN (0b11=8-bit), bit4 FEN (FIFO en)
 *   UARTCR   0x30  control: bit0 UARTEN, bit8 TXE, bit9 RXE
 *
 * Compiled only for the virt build (board.h defines BOARD_UART_PL011).
 */

#include <stdint.h>
#include "board.h"

#if defined(BOARD_UART_PL011)

#define PL011_BASE   BOARD_UART_BASE
#define UARTDR       0x00u
#define UARTFR       0x18u
#define UARTIBRD     0x24u
#define UARTFBRD     0x28u
#define UARTLCRH     0x2Cu
#define UARTCR       0x30u

#define FR_RXFE      (1u << 4)   /* receive FIFO empty */
#define FR_TXFF      (1u << 5)   /* transmit FIFO full */

#define LCRH_FEN     (1u << 4)
#define LCRH_WLEN8   (3u << 5)
#define CR_UARTEN    (1u << 0)
#define CR_TXE       (1u << 8)
#define CR_RXE       (1u << 9)

static inline void w(uint32_t off, uint32_t v) { *(volatile uint32_t *)(PL011_BASE + off) = v; }
static inline uint32_t r(uint32_t off) { return *(volatile uint32_t *)(PL011_BASE + off); }

void uart_hw_init(void)
{
	/* Disable, set 8N1 + FIFOs, re-enable TX+RX. QEMU ignores the baud divisors
	 * but we set a nominal 115200 @ 24 MHz for form (IBRD=13, FBRD=1). */
	w(UARTCR, 0);
	w(UARTIBRD, 13);
	w(UARTFBRD, 1);
	w(UARTLCRH, LCRH_WLEN8 | LCRH_FEN);
	w(UARTCR, CR_UARTEN | CR_TXE | CR_RXE);
}

void uart_hw_putc(char c)
{
	while (r(UARTFR) & FR_TXFF)      /* wait while TX FIFO full */
		;
	w(UARTDR, (uint32_t)(uint8_t)c);
}

int uart_hw_getc(void)
{
	while (r(UARTFR) & FR_RXFE)      /* wait while RX FIFO empty */
		;
	return (int)(r(UARTDR) & 0xff);
}

#endif /* BOARD_UART_PL011 */
