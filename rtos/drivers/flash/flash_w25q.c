/*
 * flash.c — W25Q128 SPI NOR flash driver
 *
 * Uses the SPI driver API — no hardcoded register addresses.
 * The SPI bus is resolved from the DTS parent node (DT_PARENT_LABEL).
 *
 * All hardware knowledge is split:
 *   - SPI driver knows how to clock bytes in/out (from DT: base, pins)
 *   - Flash driver knows the W25Q128 command protocol
 *   - Device tree connects them (flash is child of SPI node)
 */

#include <stdint.h>
#include "config.h"
#include "devicetree.h"
#include "device.h"
#include "drivers/flash.h"
#include "drivers/spi.h"
#ifdef CONFIG_PM
#include "pm.h"
#endif

/* W25Q128 commands */
#define CMD_WRITE_ENABLE  0x06
#define CMD_READ_STATUS   0x05
#define CMD_READ_DATA     0x03
#define CMD_PAGE_PROGRAM  0x02
#define CMD_SECTOR_ERASE  0x20
#define CMD_JEDEC_ID      0x9F

#define STATUS_BUSY       0x01

struct flash_nor_config {
    uint32_t size;
    uint16_t page_size;
    uint16_t sector_size;
};

/* Reference to the SPI bus this flash sits on (from DTS parent node) */
DEVICE_DT_DECLARE(DT_INST_JEDEC_SPI_NOR_0_PARENT_LABEL);

static const struct device *spi(void)
{
    return DEVICE_DT_GET(DT_INST_JEDEC_SPI_NOR_0_PARENT_LABEL);
}

#ifdef CONFIG_PM
/*
 * SPI bus PM state — the flash driver manages SPI power.
 * SPI is suspended when no flash operation is in progress.
 *
 * In a real system, the SPI driver would own this pm_device
 * and the PM callback would disable/enable the SPI clock.
 * We simplify by putting it here.
 */
static int spi_pm_action(const struct device *dev, enum pm_device_action action)
{
    (void)dev;
    /* In a real driver: disable/enable SPI clock via clock_on/clock_off */
    return 0;
}

static struct pm_device spi_pm = {
    .action_cb = spi_pm_action,
    .state = PM_DEVICE_ACTIVE,
    .usage_count = 0,
};

#define SPI_PM_GET()  pm_runtime_get(spi(), &spi_pm)
#define SPI_PM_PUT()  pm_runtime_put(spi(), &spi_pm)
#else
#define SPI_PM_GET()  (void)0
#define SPI_PM_PUT()  (void)0
#endif

/* --- W25Q128 command helpers --- */

static void w25q_write_enable(void)
{
    uint8_t cmd = CMD_WRITE_ENABLE;
    spi_cs_select(spi());
    spi_write(spi(), &cmd, 1);
    spi_cs_release(spi());
}

static void w25q_wait_busy(void)
{
    uint8_t cmd = CMD_READ_STATUS;
    uint8_t status;

    spi_cs_select(spi());
    spi_write(spi(), &cmd, 1);
    do {
        spi_read(spi(), &status, 1);
    } while (status & STATUS_BUSY);
    spi_cs_release(spi());
}

static void w25q_cmd_addr(uint8_t cmd, uint32_t addr)
{
    uint8_t buf[4] = {
        cmd,
        (addr >> 16) & 0xFF,
        (addr >> 8) & 0xFF,
        addr & 0xFF,
    };
    spi_write(spi(), buf, 4);
}

/* --- Flash driver API --- */

static int flash_nor_init(const struct device *dev)
{
    (void)dev;

    /* Verify JEDEC ID — accept Winbond W25Q128 or GigaDevice GD25Q128 */
    uint8_t cmd = CMD_JEDEC_ID;
    uint8_t id[3];

    spi_cs_select(spi());
    spi_write(spi(), &cmd, 1);
    spi_read(spi(), id, 3);
    spi_cs_release(spi());

    /* id[0]=manufacturer, id[1]=type(0x40), id[2]=capacity(0x18=128Mbit) */
    if ((id[0] != 0xEF && id[0] != 0xC8) || id[1] != 0x40 || id[2] != 0x18)
        return -1;

    return 0;
}

static int flash_nor_read(const struct device *dev, uint32_t offset,
                          void *buf, size_t len)
{
    (void)dev;

    SPI_PM_GET();

    spi_cs_select(spi());
    w25q_cmd_addr(CMD_READ_DATA, offset);
    spi_read(spi(), buf, len);
    spi_cs_release(spi());

    SPI_PM_PUT();

    return 0;
}

static int flash_nor_write(const struct device *dev, uint32_t offset,
                           const void *buf, size_t len)
{
    (void)dev;
    const uint8_t *src = buf;

    SPI_PM_GET();

    while (len > 0) {
        size_t page_remain = 256 - (offset & 0xFF);
        size_t chunk = len < page_remain ? len : page_remain;

        w25q_write_enable();

        spi_cs_select(spi());
        w25q_cmd_addr(CMD_PAGE_PROGRAM, offset);
        spi_write(spi(), src, chunk);
        spi_cs_release(spi());

        w25q_wait_busy();

        offset += chunk;
        src += chunk;
        len -= chunk;
    }

    SPI_PM_PUT();

    return 0;
}

static int flash_nor_erase(const struct device *dev, uint32_t offset,
                           size_t size)
{
    (void)dev;

    SPI_PM_GET();

    while (size > 0) {
        w25q_write_enable();

        spi_cs_select(spi());
        w25q_cmd_addr(CMD_SECTOR_ERASE, offset);
        spi_cs_release(spi());

        w25q_wait_busy();

        offset += 4096;
        size = size > 4096 ? size - 4096 : 0;
    }

    SPI_PM_PUT();

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
                     &flash_nor_api, 40);

DT_INST_FOREACH_STATUS_OKAY(JEDEC_SPI_NOR, JEDEC_SPI_NOR_DEFINE)
