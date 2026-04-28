#ifndef DTS_H
#define DTS_H

#include <stdint.h>

#define DTS_MAX_NODES 32

struct dts_node {
    char compatible[64];
    char label[32];
    uint32_t reg;
    int has_reg;
    int dc_pin;        /* -1 if not set */
    int cs_pin;        /* -1 if not set */
    uint32_t spi_bus;  /* reg of parent SPI bus, 0 if not set */
};

struct dts {
    struct dts_node nodes[DTS_MAX_NODES];
    int nnodes;
    uint32_t sysclk_hz;
};

/* Parse a .dts file. Returns 0 on success. */
int dts_parse(struct dts *d, const char *path);

/* Find first node with given compatible string. Returns NULL if not found. */
const struct dts_node *dts_find(const struct dts *d, const char *compatible);

#endif
