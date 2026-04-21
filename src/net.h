/*
 * net.h — Minimal network stack
 *
 * Provides a simple UDP-like datagram API over a loopback "driver."
 * Demonstrates the same layering as Zephyr's net subsystem:
 *
 *   Application
 *     ↓ net_send() / net_recv()
 *   Socket layer (port-based demux)
 *     ↓
 *   Network buffers (net_buf from memslab)
 *     ↓
 *   Network driver (loopback — just moves buf from TX to RX queue)
 *
 * In Zephyr this would be:
 *   BSD sockets API → net_context → net_pkt → net_if → driver
 */

#ifndef NET_H
#define NET_H

#include <stdint.h>
#include <stddef.h>

#define NET_MAX_SOCKETS 4
#define NET_BUF_SIZE    64
#define NET_NUM_BUFS    8

/* A network buffer (like Zephyr's net_buf / net_pkt) */
struct net_buf {
    uint8_t  data[NET_BUF_SIZE];
    uint16_t len;
    uint16_t src_port;
    uint16_t dst_port;
};

/* A socket (like Zephyr's net_context or BSD socket fd) */
struct net_socket {
    uint16_t port;          /* bound port */
    uint8_t  active;
};

/* Initialize the network stack */
void net_init(void);

/* Bind a socket to a port. Returns socket index or -1. */
int net_socket_open(uint16_t port);

/* Send data to a destination port */
int net_send(int sock, uint16_t dst_port, const void *data, size_t len);

/* Receive data (blocks until a packet arrives for this socket's port) */
int net_recv(int sock, void *buf, size_t buf_size);

/* Non-blocking receive. Returns bytes received or 0 if nothing. */
int net_recv_nb(int sock, void *buf, size_t buf_size);

#endif
