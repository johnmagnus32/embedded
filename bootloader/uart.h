/*
 * uart.h — UART0 console (PE2/PE3) for the gameboy-v3 bootloader.
 * Shared by main.c and the tiny printf in dram_shim.c.
 */
#ifndef GV3_UART_H
#define GV3_UART_H

void uart0_init(void);
void uart0_putc(char c);
void uart0_puts(const char *s);

#endif /* GV3_UART_H */
