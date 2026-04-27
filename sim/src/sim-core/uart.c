/*
 * uart.c — USART device
 */
#include "uart.h"
#include "cpu.h"

void uart_init(struct uart *u, uint32_t base)
{
    u->base = base;
    u->head = 0;
    u->count = 0;
}

int uart_handles(struct uart *u, uint32_t addr)
{
    return addr >= u->base && addr < u->base + 0x20;
}

uint32_t uart_read(struct uart *u, uint32_t addr)
{
    if (addr == u->base + 0x00) return (1 << 7) | (1 << 6);
    return 0;
}

void uart_write(struct uart *u, uint32_t addr, uint32_t val)
{
    if (addr != u->base + 0x04) return;
    char c = (char)(val & 0xFF);
    if (c == '\r') return;

    u->buf[u->head] = c;
    u->head = (u->head + 1) % UART_BUF_SIZE;
    if (u->count < UART_BUF_SIZE) u->count++;
}

void uart_dump_state(struct uart *u, FILE *f)
{
    fprintf(f, "{\"uart\":\"");
    int start = (u->count < UART_BUF_SIZE) ? 0 : u->head;
    for (int j = 0; j < u->count; j++) {
        char c = u->buf[(start + j) % UART_BUF_SIZE];
        if (c == '"') fprintf(f, "\\\"");
        else if (c == '\\') fprintf(f, "\\\\");
        else if (c == '\n') fprintf(f, "\\n");
        else if ((unsigned char)c >= 0x20) fputc(c, f);
    }
    fprintf(f, "\"}");
}
