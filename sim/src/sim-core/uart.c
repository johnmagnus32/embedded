/*
 * uart.c — USART2 device
 *
 * Writes its state to a file whenever output changes.
 */
#include "uart.h"
#include "cpu.h"

static char uart_state_path[256] = "";

void uart_init(struct uart *u)
{
    u->head = 0;
    u->count = 0;
}

void uart_set_state_dir(const char *dir)
{
    snprintf(uart_state_path, sizeof(uart_state_path), "%s/uart.json", dir);
}

static void uart_flush_state(struct uart *u)
{
    if (!uart_state_path[0]) return;
    FILE *f = fopen(uart_state_path, "w");
    if (!f) return;
    uart_dump_state(u, f);
    fprintf(f, "\n");
    fclose(f);
}

int uart_handles(uint32_t addr)
{
    return addr >= USART2_BASE && addr < USART2_BASE + 0x20;
}

uint32_t uart_read(struct uart *u, uint32_t addr)
{
    (void)u;
    if (addr == USART2_BASE + 0x00) return (1 << 7) | (1 << 6);
    return 0;
}

void uart_write(struct uart *u, uint32_t addr, uint32_t val)
{
    if (addr != USART2_BASE + 0x04) return;
    char c = (char)(val & 0xFF);
    if (c == '\r') return;

    u->buf[u->head] = c;
    u->head = (u->head + 1) % UART_BUF_SIZE;
    if (u->count < UART_BUF_SIZE) u->count++;

    /* Flush on newline for efficiency */
    if (c == '\n') uart_flush_state(u);
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
