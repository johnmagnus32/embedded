/*
 * net.c — Minimal network stack with loopback driver
 *
 * Architecture:
 *   net_send() → alloc net_buf from slab → copy data → enqueue to RX
 *   net_recv() → dequeue from RX queue for matching port → copy to user
 *
 * The "loopback driver" is trivial: TX just puts the buffer directly
 * into the RX queue. A real driver would DMA to an Ethernet MAC.
 *
 * The RX queue is per-port (each socket has a small ring buffer).
 * This is like Zephyr's net_context receive FIFO.
 */

#include "net.h"
#include "memslab.h"
#include "sched.h"
#include <stdint.h>

/* irq_lock for atomicity */
static inline uint32_t irq_lock(void)
{
    uint32_t key;
    __asm volatile("mrs %0, primask\n cpsid i" : "=r"(key));
    return key;
}
static inline void irq_unlock(uint32_t key)
{
    __asm volatile("msr primask, %0" :: "r"(key));
}

/* Buffer pool (like Zephyr's NET_BUF_POOL_DEFINE) */
static uint8_t buf_pool_mem[NET_NUM_BUFS * sizeof(struct net_buf)];
static struct memslab buf_pool;

/* Sockets */
static struct net_socket sockets[NET_MAX_SOCKETS];

/* Per-socket RX queue (simple ring buffer of net_buf pointers) */
#define RX_QUEUE_SIZE 4
static struct net_buf *rx_queues[NET_MAX_SOCKETS][RX_QUEUE_SIZE];
static uint8_t rx_head[NET_MAX_SOCKETS];
static uint8_t rx_tail[NET_MAX_SOCKETS];
static uint8_t rx_count[NET_MAX_SOCKETS];

void net_init(void)
{
    memslab_init(&buf_pool, buf_pool_mem,
                 sizeof(struct net_buf), NET_NUM_BUFS);

    for (int i = 0; i < NET_MAX_SOCKETS; i++) {
        sockets[i].active = 0;
        rx_head[i] = 0;
        rx_tail[i] = 0;
        rx_count[i] = 0;
    }
}

int net_socket_open(uint16_t port)
{
    for (int i = 0; i < NET_MAX_SOCKETS; i++) {
        if (!sockets[i].active) {
            sockets[i].port = port;
            sockets[i].active = 1;
            return i;
        }
    }
    return -1;
}

/* Find socket index by port */
static int find_socket_by_port(uint16_t port)
{
    for (int i = 0; i < NET_MAX_SOCKETS; i++) {
        if (sockets[i].active && sockets[i].port == port)
            return i;
    }
    return -1;
}

/* Loopback "driver": deliver packet to the destination socket's RX queue */
static void loopback_tx(struct net_buf *buf)
{
    int dst_sock = find_socket_by_port(buf->dst_port);
    if (dst_sock < 0) {
        /* No socket listening — drop packet */
        memslab_free(&buf_pool, buf);
        return;
    }

    uint32_t key = irq_lock();
    if (rx_count[dst_sock] < RX_QUEUE_SIZE) {
        rx_queues[dst_sock][rx_head[dst_sock]] = buf;
        rx_head[dst_sock] = (rx_head[dst_sock] + 1) % RX_QUEUE_SIZE;
        rx_count[dst_sock]++;
    } else {
        /* Queue full — drop */
        memslab_free(&buf_pool, buf);
    }
    irq_unlock(key);
}

int net_send(int sock, uint16_t dst_port, const void *data, size_t len)
{
    if (sock < 0 || sock >= NET_MAX_SOCKETS || !sockets[sock].active)
        return -1;
    if (len > NET_BUF_SIZE)
        len = NET_BUF_SIZE;

    /* Allocate a network buffer from the pool */
    struct net_buf *buf = memslab_alloc(&buf_pool);
    if (!buf) return -1;  /* out of buffers */

    /* Fill the buffer */
    uint8_t *s = (uint8_t *)data;
    for (size_t i = 0; i < len; i++)
        buf->data[i] = s[i];
    buf->len = len;
    buf->src_port = sockets[sock].port;
    buf->dst_port = dst_port;

    /* "Transmit" — loopback delivers directly to RX */
    loopback_tx(buf);
    return (int)len;
}

int net_recv(int sock, void *out, size_t buf_size)
{
    if (sock < 0 || sock >= NET_MAX_SOCKETS)
        return -1;

    /* Block until a packet arrives */
    while (1) {
        uint32_t key = irq_lock();
        if (rx_count[sock] > 0) {
            struct net_buf *buf = rx_queues[sock][rx_tail[sock]];
            rx_tail[sock] = (rx_tail[sock] + 1) % RX_QUEUE_SIZE;
            rx_count[sock]--;
            irq_unlock(key);

            /* Copy to user buffer */
            size_t copy_len = buf->len < buf_size ? buf->len : buf_size;
            uint8_t *d = (uint8_t *)out;
            for (size_t i = 0; i < copy_len; i++)
                d[i] = buf->data[i];

            /* Free the network buffer back to the pool */
            memslab_free(&buf_pool, buf);
            return (int)copy_len;
        }
        irq_unlock(key);
        sched_yield();
    }
}

int net_recv_nb(int sock, void *out, size_t buf_size)
{
    if (sock < 0 || sock >= NET_MAX_SOCKETS)
        return -1;

    uint32_t key = irq_lock();
    if (rx_count[sock] == 0) {
        irq_unlock(key);
        return 0;
    }

    struct net_buf *buf = rx_queues[sock][rx_tail[sock]];
    rx_tail[sock] = (rx_tail[sock] + 1) % RX_QUEUE_SIZE;
    rx_count[sock]--;
    irq_unlock(key);

    size_t copy_len = buf->len < buf_size ? buf->len : buf_size;
    uint8_t *d = (uint8_t *)out;
    for (size_t i = 0; i < copy_len; i++)
        d[i] = buf->data[i];

    memslab_free(&buf_pool, buf);
    return (int)copy_len;
}
