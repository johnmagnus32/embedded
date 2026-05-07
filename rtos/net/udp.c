/*
 * udp.c — Minimal UDP/IP stack with ARP
 *
 * Handles:
 *   - Ethernet frame build/parse
 *   - ARP request/reply (resolve IP → MAC)
 *   - IPv4 header build/parse + checksum
 *   - UDP header build/parse
 *   - Simple socket demux by port
 *
 * ~300 lines for a working UDP/IP stack.
 * Compare to lwIP (~15,000 lines) or Zephyr's net subsys (~50,000 lines).
 */

#include "net.h"
#include "config.h"
#include <stdint.h>

#ifdef CONFIG_NET

/* Network state */
static const struct device *net_eth_dev;
static ip_addr_t  net_ip;
static ip_addr_t  net_netmask;
static ip_addr_t  net_gateway;
static struct mac_addr net_mac;

/* Sockets */
static struct udp_socket sockets[NET_MAX_SOCKETS];

/* ARP cache (tiny: 4 entries) */
#define ARP_CACHE_SIZE 4
static struct {
    ip_addr_t ip;
    struct mac_addr mac;
    uint8_t valid;
} arp_cache[ARP_CACHE_SIZE];

/* RX socket buffers (one pending packet per socket) */
static struct {
    uint8_t  data[256];
    uint16_t len;
    ip_addr_t src_ip;
    uint16_t src_port;
    uint8_t  ready;
} rx_buf[NET_MAX_SOCKETS];

/* Packet buffer */
static uint8_t pkt_buf[600];

/* ---- Helpers ---- */

static uint16_t htons(uint16_t h) { return (h >> 8) | (h << 8); }
static uint32_t htonl(uint32_t h) {
    return ((h >> 24) & 0xFF) | ((h >> 8) & 0xFF00) |
           ((h << 8) & 0xFF0000) | ((h << 24) & 0xFF000000);
}
#define ntohs htons
#define ntohl htonl

static void mcpy(void *d, const void *s, size_t n) {
    uint8_t *dd = d; const uint8_t *ss = s;
    while (n--) *dd++ = *ss++;
}

static int meq(const void *a, const void *b, size_t n) {
    const uint8_t *aa = a, *bb = b;
    while (n--) { if (*aa++ != *bb++) return 0; }
    return 1;
}

static uint16_t ip_checksum(const void *data, size_t len)
{
    const uint16_t *p = data;
    uint32_t sum = 0;
    while (len > 1) { sum += *p++; len -= 2; }
    if (len) sum += *(const uint8_t *)p;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}

/* ---- ARP ---- */

struct arp_pkt {
    uint16_t htype;     /* 1 = Ethernet */
    uint16_t ptype;     /* 0x0800 = IPv4 */
    uint8_t  hlen;      /* 6 */
    uint8_t  plen;      /* 4 */
    uint16_t oper;      /* 1 = request, 2 = reply */
    uint8_t  sha[6];    /* sender MAC */
    uint32_t spa;       /* sender IP */
    uint8_t  tha[6];    /* target MAC */
    uint32_t tpa;       /* target IP */
} __attribute__((packed));

static void arp_cache_add(ip_addr_t ip, const uint8_t *mac)
{
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!arp_cache[i].valid || arp_cache[i].ip == ip) {
            arp_cache[i].ip = ip;
            mcpy(arp_cache[i].mac.addr, mac, 6);
            arp_cache[i].valid = 1;
            return;
        }
    }
    /* Cache full — overwrite first entry */
    arp_cache[0].ip = ip;
    mcpy(arp_cache[0].mac.addr, mac, 6);
}

static int arp_cache_lookup(ip_addr_t ip, struct mac_addr *mac)
{
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            mcpy(mac->addr, arp_cache[i].mac.addr, 6);
            return 0;
        }
    }
    return -1;
}

static void arp_send_request(ip_addr_t target_ip)
{
    uint8_t frame[14 + sizeof(struct arp_pkt)];
    struct eth_hdr *eth = (struct eth_hdr *)frame;
    struct arp_pkt *arp = (struct arp_pkt *)(frame + 14);

    /* Broadcast Ethernet frame */
    uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    mcpy(eth->dst, bcast, 6);
    mcpy(eth->src, net_mac.addr, 6);
    eth->ethertype = htons(ETH_TYPE_ARP);

    arp->htype = htons(1);
    arp->ptype = htons(0x0800);
    arp->hlen = 6;
    arp->plen = 4;
    arp->oper = htons(1);  /* request */
    mcpy(arp->sha, net_mac.addr, 6);
    arp->spa = htonl(net_ip);
    uint8_t zero[6] = {0};
    mcpy(arp->tha, zero, 6);
    arp->tpa = htonl(target_ip);

    eth_send(net_eth_dev, frame, sizeof(frame));
}

static void arp_handle(const uint8_t *frame, size_t len)
{
    if (len < 14 + sizeof(struct arp_pkt)) return;
    struct arp_pkt *arp = (struct arp_pkt *)(frame + 14);

    /* Learn sender's MAC */
    arp_cache_add(ntohl(arp->spa), arp->sha);

    if (ntohs(arp->oper) == 1 && ntohl(arp->tpa) == net_ip) {
        /* ARP request for us — send reply */
        uint8_t reply[14 + sizeof(struct arp_pkt)];
        struct eth_hdr *reth = (struct eth_hdr *)reply;
        struct arp_pkt *rarp = (struct arp_pkt *)(reply + 14);

        mcpy(reth->dst, arp->sha, 6);
        mcpy(reth->src, net_mac.addr, 6);
        reth->ethertype = htons(ETH_TYPE_ARP);

        rarp->htype = htons(1);
        rarp->ptype = htons(0x0800);
        rarp->hlen = 6;
        rarp->plen = 4;
        rarp->oper = htons(2);  /* reply */
        mcpy(rarp->sha, net_mac.addr, 6);
        rarp->spa = htonl(net_ip);
        mcpy(rarp->tha, arp->sha, 6);
        rarp->tpa = arp->spa;

        eth_send(net_eth_dev, reply, sizeof(reply));
    }
}

/* ---- IP + UDP TX ---- */

static int resolve_mac(ip_addr_t dst_ip, struct mac_addr *dst_mac)
{
    /* If not on our subnet, use gateway */
    ip_addr_t target = dst_ip;
    if ((dst_ip & net_netmask) != (net_ip & net_netmask))
        target = net_gateway;

    if (arp_cache_lookup(target, dst_mac) == 0)
        return 0;

    /* Send ARP request and fail (caller should retry) */
    arp_send_request(target);
    return -1;
}

int udp_send(int sock, ip_addr_t dst_ip, uint16_t dst_port,
             const void *data, size_t len)
{
    if (sock < 0 || sock >= NET_MAX_SOCKETS || !sockets[sock].bound)
        return -1;
    if (len > 512) len = 512;

    struct mac_addr dst_mac;
    if (resolve_mac(dst_ip, &dst_mac) < 0)
        return -1;  /* ARP not resolved yet */

    uint8_t frame[14 + 20 + 8 + 512];
    size_t total = 14 + 20 + 8 + len;

    /* Ethernet header */
    struct eth_hdr *eth = (struct eth_hdr *)frame;
    mcpy(eth->dst, dst_mac.addr, 6);
    mcpy(eth->src, net_mac.addr, 6);
    eth->ethertype = htons(ETH_TYPE_IP);

    /* IP header */
    struct ip_hdr *ip = (struct ip_hdr *)(frame + 14);
    ip->ver_ihl = 0x45;
    ip->tos = 0;
    ip->total_len = htons(20 + 8 + len);
    ip->id = 0;
    ip->flags_frag = 0;
    ip->ttl = 64;
    ip->protocol = IP_PROTO_UDP;
    ip->checksum = 0;
    ip->src_ip = htonl(net_ip);
    ip->dst_ip = htonl(dst_ip);
    ip->checksum = ip_checksum(ip, 20);

    /* UDP header */
    struct udp_hdr *udp = (struct udp_hdr *)(frame + 14 + 20);
    udp->src_port = htons(sockets[sock].local_port);
    udp->dst_port = htons(dst_port);
    udp->length = htons(8 + len);
    udp->checksum = 0;  /* optional for UDP over IPv4 */

    /* Payload */
    mcpy(frame + 14 + 20 + 8, data, len);

    return eth_send(net_eth_dev, frame, total);
}

/* ---- IP + UDP RX ---- */

static void udp_handle(const uint8_t *frame, size_t len)
{
    if (len < 14 + 20 + 8) return;

    struct ip_hdr *ip = (struct ip_hdr *)(frame + 14);
    if (ip->protocol != IP_PROTO_UDP) return;
    if (ntohl(ip->dst_ip) != net_ip) return;

    struct udp_hdr *udp = (struct udp_hdr *)(frame + 14 + 20);
    uint16_t dst_port = ntohs(udp->dst_port);
    uint16_t data_len = ntohs(udp->length) - 8;
    const uint8_t *payload = frame + 14 + 20 + 8;

    /* Find socket for this port */
    for (int i = 0; i < NET_MAX_SOCKETS; i++) {
        if (sockets[i].bound && sockets[i].local_port == dst_port && !rx_buf[i].ready) {
            size_t copy = data_len > 256 ? 256 : data_len;
            mcpy(rx_buf[i].data, payload, copy);
            rx_buf[i].len = copy;
            rx_buf[i].src_ip = ntohl(ip->src_ip);
            rx_buf[i].src_port = ntohs(udp->src_port);
            rx_buf[i].ready = 1;
            return;
        }
    }
}

/* ---- Public API ---- */

void net_init(const struct device *eth_dev, ip_addr_t ip, ip_addr_t netmask, ip_addr_t gateway)
{
    net_eth_dev = eth_dev;
    net_ip = ip;
    net_netmask = netmask;
    net_gateway = gateway;
    eth_get_mac(eth_dev, &net_mac);
}

int udp_socket_open(uint16_t port)
{
    for (int i = 0; i < NET_MAX_SOCKETS; i++) {
        if (!sockets[i].bound) {
            sockets[i].local_port = port;
            sockets[i].bound = 1;
            rx_buf[i].ready = 0;
            return i;
        }
    }
    return -1;
}

int udp_recv(int sock, void *buf, size_t buf_size,
             ip_addr_t *src_ip, uint16_t *src_port)
{
    if (sock < 0 || sock >= NET_MAX_SOCKETS) return -1;
    if (!rx_buf[sock].ready) return 0;

    size_t copy = rx_buf[sock].len < buf_size ? rx_buf[sock].len : buf_size;
    mcpy(buf, rx_buf[sock].data, copy);
    if (src_ip) *src_ip = rx_buf[sock].src_ip;
    if (src_port) *src_port = rx_buf[sock].src_port;
    rx_buf[sock].ready = 0;
    return (int)copy;
}

void net_poll(void)
{
    int len = eth_recv(net_eth_dev, pkt_buf, sizeof(pkt_buf));
    if (len < 14) return;

    struct eth_hdr *eth = (struct eth_hdr *)pkt_buf;
    uint16_t type = ntohs(eth->ethertype);

    if (type == ETH_TYPE_ARP)
        arp_handle(pkt_buf, len);
    else if (type == ETH_TYPE_IP)
        udp_handle(pkt_buf, len);
}

#endif /* CONFIG_NET */
