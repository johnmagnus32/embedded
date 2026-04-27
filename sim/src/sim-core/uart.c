/*
 * uart.c — USART device
 *
 * TX bytes are streamed to a connected TCP client in real time,
 * just like QEMU's chardev backend.
 */
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "uart.h"
#include "cpu.h"

static int uart_srv_fd = -1;

void uart_init(struct uart *u, uint32_t base)
{
    u->base = base;
    u->client_fd = -1;
}

int uart_listen(struct uart *u, int port)
{
    (void)u;
    uart_srv_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(uart_srv_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK)
    };
    if (bind(uart_srv_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        return -1;
    listen(uart_srv_fd, 1);
    return port;
}

void uart_accept(struct uart *u)
{
    if (uart_srv_fd >= 0)
        u->client_fd = accept(uart_srv_fd, NULL, NULL);
}

int uart_handles(struct uart *u, uint32_t addr)
{
    return addr >= u->base && addr < u->base + 0x20;
}

uint32_t uart_read(struct uart *u, uint32_t addr)
{
    if (addr == u->base + 0x00) return (1 << 7) | (1 << 6);  /* TXE+TC */
    return 0;
}

void uart_write(struct uart *u, uint32_t addr, uint32_t val)
{
    if (addr != u->base + 0x04) return;
    char c = (char)(val & 0xFF);

    /* Stream byte to connected client */
    if (u->client_fd >= 0) {
        if (write(u->client_fd, &c, 1) <= 0)
            u->client_fd = -1;  /* client disconnected */
    }
}
