/*
 * net.h — Minimal UDP/IP networking API
 *
 * Like Zephyr's net/socket.h but only UDP.
 * No TCP, no connection state, no retransmission.
 */

#ifndef NET_H
#define NET_H

#include <stdint.h>
#include <stddef.h>
#include "device.h"

/* MAC address */
struct mac_addr {
    uint8_t addr[6];
};

/* IPv4 address */
typedef uint32_t ip_addr_t;

#define IP_ADDR(a, b, c, d) \
    ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (d))

/* ---- Protocol headers (packed, match wire format) ---- */

struct eth_hdr {
    uint8_t  dst[6];
    uint8_t  src[6];
    uint16_t ethertype;
} __attribute__((packed));

#define ETH_TYPE_IP   0x0800
#define ETH_TYPE_ARP  0x0806

struct ip_hdr {
    uint8_t  ver_ihl;      /* version (4) + IHL (5) = 0x45 */
    uint8_t  tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_frag;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
} __attribute__((packed));

#define IP_PROTO_UDP  17

struct udp_hdr {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed));

/* ---- Ethernet driver API ---- */

struct eth_driver_api {
    int (*send)(const struct device *dev, const void *frame, size_t len);
    int (*recv)(const struct device *dev, void *buf, size_t buf_size);
    void (*get_mac)(const struct device *dev, struct mac_addr *mac);
};

static inline int eth_send(const struct device *dev, const void *frame, size_t len)
{
    const struct eth_driver_api *api = dev->api;
    return api->send(dev, frame, len);
}

static inline int eth_recv(const struct device *dev, void *buf, size_t buf_size)
{
    const struct eth_driver_api *api = dev->api;
    return api->recv(dev, buf, buf_size);
}

static inline void eth_get_mac(const struct device *dev, struct mac_addr *mac)
{
    const struct eth_driver_api *api = dev->api;
    api->get_mac(dev, mac);
}

/* ---- UDP socket API ---- */

#define NET_MAX_SOCKETS 4
#define NET_MTU 576  /* minimum IP MTU */

struct udp_socket {
    uint16_t local_port;
    uint8_t  bound;
};

/* Initialize the network stack */
void net_init(const struct device *eth_dev, ip_addr_t ip, ip_addr_t netmask, ip_addr_t gateway);

/* Open a UDP socket bound to a port */
int udp_socket_open(uint16_t port);

/* Send UDP datagram */
int udp_send(int sock, ip_addr_t dst_ip, uint16_t dst_port,
             const void *data, size_t len);

/* Receive UDP datagram (non-blocking, returns 0 if nothing) */
int udp_recv(int sock, void *buf, size_t buf_size,
             ip_addr_t *src_ip, uint16_t *src_port);

/* Process one incoming packet (call from a task loop) */
void net_poll(void);

#endif
