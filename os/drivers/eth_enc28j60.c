/*
 * eth_enc28j60.c — Microchip ENC28J60 SPI Ethernet driver
 *
 * 10 Mbps Ethernet controller with 8KB buffer, connected via SPI.
 * Zephyr equivalent: drivers/ethernet/eth_enc28j60.c
 *
 * ENC28J60 SPI protocol:
 *   - Read Control Register:  0x00 | (reg & 0x1F)
 *   - Write Control Register: 0x40 | (reg & 0x1F)
 *   - Read Buffer Memory:     0x3A
 *   - Write Buffer Memory:    0x7A
 *   - Bit Field Set:          0x80 | (reg & 0x1F)
 *   - Bit Field Clear:        0xA0 | (reg & 0x1F)
 *   - System Reset:           0xFF
 */

#include <stdint.h>
#include "config.h"

#ifdef CONFIG_ETH_ENC28J60

#include "devicetree.h"
#include "device.h"
#include "net.h"
#include "spi.h"

static void mcpy(void *d, const void *s, size_t n) {
    uint8_t *dd = d; const uint8_t *ss = s;
    while (n--) *dd++ = *ss++;
}

/* ENC28J60 registers (bank 0) */
#define ERDPTL   0x00
#define ERDPTH   0x01
#define EWRPTL   0x02
#define EWRPTH   0x03
#define ERXSTL   0x08
#define ERXSTH   0x09
#define ERXNDL   0x0C
#define ERXNDH   0x0D
#define ERXRDPTL 0x0C
#define ERXRDPTH 0x0D
#define EPKTCNT  0x19  /* bank 1 */

/* Control register */
#define ECON1    0x1F
#define ECON2    0x1E
#define EIR      0x1C

/* MAC registers (bank 2) */
#define MACON1   0x00
#define MACON3   0x02
#define MAMXFLL  0x0A
#define MAMXFLH  0x0B
#define MAADR1   0x04  /* bank 3 */
#define MAADR2   0x05
#define MAADR3   0x02
#define MAADR4   0x03
#define MAADR5   0x00
#define MAADR6   0x01

/* SPI opcodes */
#define OP_RCR  0x00
#define OP_WCR  0x40
#define OP_RBM  0x3A
#define OP_WBM  0x7A
#define OP_BFS  0x80
#define OP_BFC  0xA0
#define OP_RESET 0xFF

struct enc28j60_config {
    struct mac_addr mac;
};

DEVICE_DT_DECLARE(spi1);

static const struct device *spi_dev(void) { return DEVICE_DT_GET(spi1); }

static void enc_write_op(uint8_t op, uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = { op | (reg & 0x1F), data };
    spi_cs_select(spi_dev());
    spi_write(spi_dev(), buf, 2);
    spi_cs_release(spi_dev());
}

static uint8_t enc_read_op(uint8_t op, uint8_t reg)
{
    uint8_t cmd = op | (reg & 0x1F);
    uint8_t val;
    spi_cs_select(spi_dev());
    spi_write(spi_dev(), &cmd, 1);
    spi_read(spi_dev(), &val, 1);
    spi_cs_release(spi_dev());
    return val;
}

static void enc_write_buf(const void *data, size_t len)
{
    uint8_t cmd = OP_WBM;
    spi_cs_select(spi_dev());
    spi_write(spi_dev(), &cmd, 1);
    spi_write(spi_dev(), data, len);
    spi_cs_release(spi_dev());
}

static void enc_read_buf(void *buf, size_t len)
{
    uint8_t cmd = OP_RBM;
    spi_cs_select(spi_dev());
    spi_write(spi_dev(), &cmd, 1);
    spi_read(spi_dev(), buf, len);
    spi_cs_release(spi_dev());
}

static int enc28j60_init(const struct device *dev)
{
    const struct enc28j60_config *cfg = dev->config;
    (void)cfg;

    /* Reset */
    uint8_t rst = OP_RESET;
    spi_cs_select(spi_dev());
    spi_write(spi_dev(), &rst, 1);
    spi_cs_release(spi_dev());

    /* Wait for reset (~1ms) */
    for (volatile int i = 0; i < 100000; i++);

    /* Configure RX buffer: 0x0000-0x19FF (6.5KB for RX) */
    enc_write_op(OP_WCR, ERXSTL, 0x00);
    enc_write_op(OP_WCR, ERXSTH, 0x00);
    enc_write_op(OP_WCR, ERXNDL, 0xFF);
    enc_write_op(OP_WCR, ERXNDH, 0x19);

    /* Enable MAC receive, full duplex */
    /* Bank 2 */
    enc_write_op(OP_BFC, ECON1, 0x03);  /* clear bank bits */
    enc_write_op(OP_BFS, ECON1, 0x02);  /* select bank 2 */
    enc_write_op(OP_WCR, MACON1, 0x0D); /* MARXEN + TXPAUS + RXPAUS */
    enc_write_op(OP_WCR, MACON3, 0x32); /* PADCFG + TXCRCEN + FRMLNEN */

    /* Set MAC address (bank 3) */
    enc_write_op(OP_BFC, ECON1, 0x03);
    enc_write_op(OP_BFS, ECON1, 0x03);  /* bank 3 */
    enc_write_op(OP_WCR, MAADR1, cfg->mac.addr[0]);
    enc_write_op(OP_WCR, MAADR2, cfg->mac.addr[1]);
    enc_write_op(OP_WCR, MAADR3, cfg->mac.addr[2]);
    enc_write_op(OP_WCR, MAADR4, cfg->mac.addr[3]);
    enc_write_op(OP_WCR, MAADR5, cfg->mac.addr[4]);
    enc_write_op(OP_WCR, MAADR6, cfg->mac.addr[5]);

    /* Enable receive (bank 0) */
    enc_write_op(OP_BFC, ECON1, 0x03);
    enc_write_op(OP_BFS, ECON1, 0x04);  /* RXEN */

    return 0;
}

static int enc28j60_send(const struct device *dev, const void *frame, size_t len)
{
    (void)dev;

    /* Set write pointer to TX buffer start (0x1A00) */
    enc_write_op(OP_WCR, EWRPTL, 0x00);
    enc_write_op(OP_WCR, EWRPTH, 0x1A);

    /* Write per-packet control byte + frame */
    uint8_t ctrl = 0x00;
    enc_write_buf(&ctrl, 1);
    enc_write_buf(frame, len);

    /* Set TX end pointer and start transmission */
    uint16_t end = 0x1A00 + 1 + len;
    enc_write_op(OP_WCR, 0x04, end & 0xFF);       /* ETXNDL */
    enc_write_op(OP_WCR, 0x05, (end >> 8) & 0xFF); /* ETXNDH */
    enc_write_op(OP_BFS, ECON1, 0x08);  /* TXRTS — start TX */

    return (int)len;
}

static int enc28j60_recv(const struct device *dev, void *buf, size_t buf_size)
{
    (void)dev;

    /* Check packet count (bank 1) */
    enc_write_op(OP_BFC, ECON1, 0x03);
    enc_write_op(OP_BFS, ECON1, 0x01);  /* bank 1 */
    uint8_t count = enc_read_op(OP_RCR, EPKTCNT);
    if (count == 0) return 0;

    /* Read packet header (6 bytes: next_ptr[2], len[2], status[2]) */
    uint8_t hdr[6];
    enc_read_buf(hdr, 6);
    uint16_t pkt_len = hdr[2] | (hdr[3] << 8);

    if (pkt_len > buf_size) pkt_len = buf_size;
    enc_read_buf(buf, pkt_len);

    /* Decrement packet count */
    enc_write_op(OP_BFS, ECON2, 0x40);  /* PKTDEC */

    return (int)pkt_len;
}

static void enc28j60_get_mac(const struct device *dev, struct mac_addr *mac)
{
    const struct enc28j60_config *cfg = dev->config;
    mcpy(mac->addr, cfg->mac.addr, 6);
}


static const struct eth_driver_api enc28j60_api = {
    .send = enc28j60_send,
    .recv = enc28j60_recv,
    .get_mac = enc28j60_get_mac,
};

/* Hardcoded MAC for now — in real system this comes from DT or OTP */
static const struct enc28j60_config enc28j60_cfg = {
    .mac = {{ 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 }},
};

DEVICE_DT_DEFINE(eth0, enc28j60_init, NULL, &enc28j60_cfg, &enc28j60_api);

#endif /* CONFIG_ETH_ENC28J60 */
