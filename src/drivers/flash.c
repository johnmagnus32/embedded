/*
 * flash.c — QSPI NOR flash driver
 *
 * On real hardware, this would send QSPI commands to the W25Q128:
 *   - Read:  0x03 (or 0x6B for quad fast read)
 *   - Write: 0x02 (page program, max 256 bytes)
 *   - Erase: 0x20 (4KB sector erase)
 *   - WREN:  0x06 (write enable, required before write/erase)
 *   - RDSR:  0x05 (read status, poll until not busy)
 *
 * For simulation without hardware, we back the flash with a RAM buffer.
 * The erase/write semantics are enforced (erase sets 0xFF, write can
 * only clear bits) so the behavior matches real NOR flash.
 *
 * Maps to Zephyr's drivers/flash/spi_nor.c
 */

#include <stdint.h>
#include "devicetree.h"
#include "device.h"
#include "drivers/flash.h"

/* Simulated flash storage (4KB — one sector for demo) */
#define SIM_FLASH_SIZE 4096
static uint8_t sim_flash[SIM_FLASH_SIZE];

struct flash_nor_config {
    uint32_t size;          /* total flash size in bytes */
    uint16_t page_size;     /* write page size */
    uint16_t sector_size;   /* erase sector size */
};

static int flash_nor_init(const struct device *dev)
{
    (void)dev;
    /* Simulate power-on state: all 0xFF (erased) */
    for (int i = 0; i < SIM_FLASH_SIZE; i++)
        sim_flash[i] = 0xFF;
    return 0;
}

static int flash_nor_read(const struct device *dev, uint32_t offset,
                          void *buf, size_t len)
{
    (void)dev;
    if (offset + len > SIM_FLASH_SIZE) return -1;

    /*
     * Real hardware: send QSPI read command
     *   QSPI_CR = ... (configure quad mode)
     *   QSPI_AR = offset
     *   QSPI_DLR = len - 1
     *   DMA transfer from QSPI data register to buf
     */
    uint8_t *d = buf;
    for (size_t i = 0; i < len; i++)
        d[i] = sim_flash[offset + i];

    return 0;
}

static int flash_nor_write(const struct device *dev, uint32_t offset,
                           const void *buf, size_t len)
{
    (void)dev;
    if (offset + len > SIM_FLASH_SIZE) return -1;

    /*
     * Real hardware: page program (max 256 bytes per command)
     *   Send 0x06 (WREN)
     *   Send 0x02 + 3-byte address + data
     *   Poll RDSR until WIP bit clears (~1-3ms)
     *
     * NOR flash can only clear bits (1→0). Writing 0 to a 0 is fine,
     * writing 1 to a 0 requires erase first.
     */
    const uint8_t *s = buf;
    for (size_t i = 0; i < len; i++)
        sim_flash[offset + i] &= s[i];  /* can only clear bits */

    return 0;
}

static int flash_nor_erase(const struct device *dev, uint32_t offset,
                           size_t size)
{
    (void)dev;
    if (offset + size > SIM_FLASH_SIZE) return -1;

    /*
     * Real hardware: sector erase
     *   Send 0x06 (WREN)
     *   Send 0x20 + 3-byte address
     *   Poll RDSR until WIP bit clears (~50-400ms)
     *
     * Erase sets all bits to 1 (0xFF).
     */
    for (size_t i = 0; i < size; i++)
        sim_flash[offset + i] = 0xFF;

    return 0;
}

static const struct flash_driver_api flash_nor_api = {
    .read = flash_nor_read,
    .write = flash_nor_write,
    .erase = flash_nor_erase,
};

/* ---- DT_INST instantiation ---- */

#define _FLASH_INST_LABEL(n) DT_INST_JEDEC_SPI_NOR_##n##_LABEL

#define JEDEC_SPI_NOR_DEFINE(n)                                     \
    static const struct flash_nor_config flash_cfg_##n = {          \
        .size = DT_INST_JEDEC_SPI_NOR_##n##_PROP_SIZE / 8,         \
        .page_size = DT_INST_JEDEC_SPI_NOR_##n##_PROP_PAGE_SIZE,   \
        .sector_size = DT_INST_JEDEC_SPI_NOR_##n##_PROP_SECTOR_SIZE, \
    };                                                              \
    DEVICE_DT_DEFINE(_FLASH_INST_LABEL(n),                          \
                     flash_nor_init, NULL, &flash_cfg_##n,           \
                     &flash_nor_api);

DT_INST_FOREACH_STATUS_OKAY(JEDEC_SPI_NOR, JEDEC_SPI_NOR_DEFINE)
