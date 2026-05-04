#include "w25q128.h"
#include "spi_bus.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void w25q128_init(struct w25q128 *dev)
{
    memset(dev, 0, sizeof(*dev));
    dev->data = malloc(W25Q128_SIZE);
    memset(dev->data, 0xFF, W25Q128_SIZE);  /* erased state */
    dev->state = W25Q_IDLE;
}

void w25q128_free(struct w25q128 *dev)
{
    free(dev->data);
    dev->data = NULL;
}

int w25q128_load(struct w25q128 *dev, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    size_t n = fread(dev->data, 1, W25Q128_SIZE, f);
    fclose(f);
    return (int)n;
}

static int is_busy(struct w25q128 *dev)
{
    if (!(dev->status & W25Q_STATUS_BUSY)) return 0;
    if (dev->cycle_count_ptr && *dev->cycle_count_ptr >= dev->busy_until) {
        dev->status &= ~W25Q_STATUS_BUSY;
        return 0;
    }
    return 1;
}

static void commit_page_program(struct w25q128 *dev)
{
    if (dev->page_len == 0) return;
    uint32_t base = dev->page_addr & ~(W25Q128_PAGE_SIZE - 1);
    uint32_t offset = dev->page_addr & (W25Q128_PAGE_SIZE - 1);
    for (int i = 0; i < dev->page_len; i++) {
        uint32_t addr = base + ((offset + i) & (W25Q128_PAGE_SIZE - 1));
        if (addr < W25Q128_SIZE)
            dev->data[addr] &= dev->page_buf[i];  /* can only clear bits */
    }
    dev->status &= ~W25Q_STATUS_WEL;
    /* Page program takes ~0.7ms = ~11200 cycles at 16MHz */
    if (dev->cycle_count_ptr)
        dev->busy_until = *dev->cycle_count_ptr + 11200;
    dev->status |= W25Q_STATUS_BUSY;
    dev->page_len = 0;
}

static void commit_sector_erase(struct w25q128 *dev)
{
    uint32_t base = dev->addr & ~(W25Q128_SECTOR_SIZE - 1);
    if (base + W25Q128_SECTOR_SIZE <= W25Q128_SIZE)
        memset(&dev->data[base], 0xFF, W25Q128_SECTOR_SIZE);
    dev->status &= ~W25Q_STATUS_WEL;
    /* Sector erase takes ~45ms = ~720000 cycles at 16MHz */
    if (dev->cycle_count_ptr)
        dev->busy_until = *dev->cycle_count_ptr + 720000;
    dev->status |= W25Q_STATUS_BUSY;
}

void w25q128_cs_deassert(struct w25q128 *dev)
{
    /* CS going high ends the current command */
    if (dev->state == W25Q_PROGRAM_DATA)
        commit_page_program(dev);
    dev->state = W25Q_IDLE;
}

void w25q128_cs_handler(void *opaque, int level)
{
    struct w25q128 *dev = (struct w25q128 *)opaque;
    /* CS is active-low: level=0 means selected, level=1 means deasserted */
    if (dev->spi_slave)
        dev->spi_slave->cs_active = !level;
    if (level)
        w25q128_cs_deassert(dev);
}

uint8_t w25q128_transfer(void *opaque, uint8_t byte)
{
    struct w25q128 *dev = (struct w25q128 *)opaque;

    /* If busy, only status read works */
    if (is_busy(dev) && dev->state != W25Q_READ_STATUS) {
        if (dev->state == W25Q_IDLE) {
            if (byte == W25Q_CMD_READ_STATUS) {
                dev->state = W25Q_READ_STATUS;
                return 0xFF;
            }
            return 0xFF;
        }
        return 0xFF;
    }

    switch (dev->state) {
    case W25Q_IDLE:
        dev->cmd = byte;
        dev->addr = 0;
        dev->addr_bytes = 0;
        switch (byte) {
        case W25Q_CMD_JEDEC_ID:
            dev->state = W25Q_JEDEC_ID;
            dev->jedec_idx = 0;
            break;
        case W25Q_CMD_READ_STATUS:
            dev->state = W25Q_READ_STATUS;
            break;
        case W25Q_CMD_WRITE_ENABLE:
            dev->status |= W25Q_STATUS_WEL;
            break;
        case W25Q_CMD_WRITE_DISABLE:
            dev->status &= ~W25Q_STATUS_WEL;
            break;
        case W25Q_CMD_READ_DATA:
            dev->state = W25Q_READ_ADDR;
            break;
        case W25Q_CMD_FAST_READ:
            dev->state = W25Q_FAST_READ_ADDR;
            break;
        case W25Q_CMD_PAGE_PROGRAM:
            if (dev->status & W25Q_STATUS_WEL)
                dev->state = W25Q_PROGRAM_ADDR;
            break;
        case W25Q_CMD_SECTOR_ERASE:
            if (dev->status & W25Q_STATUS_WEL)
                dev->state = W25Q_ERASE_ADDR;
            break;
        }
        return 0xFF;

    case W25Q_JEDEC_ID: {
        static const uint8_t id[] = {W25Q128_MFR_ID, W25Q128_DEV_TYPE, W25Q128_DEV_CAP};
        uint8_t r = (dev->jedec_idx < 3) ? id[dev->jedec_idx] : 0x00;
        dev->jedec_idx++;
        return r;
    }

    case W25Q_READ_STATUS:
        is_busy(dev);  /* refresh busy flag */
        return dev->status;

    case W25Q_READ_ADDR:
        dev->addr = (dev->addr << 8) | byte;
        if (++dev->addr_bytes == 3)
            dev->state = W25Q_READ_DATA;
        return 0xFF;

    case W25Q_READ_DATA: {
        uint8_t r = (dev->addr < W25Q128_SIZE) ? dev->data[dev->addr] : 0xFF;
        dev->addr++;
        return r;
    }

    case W25Q_FAST_READ_ADDR:
        dev->addr = (dev->addr << 8) | byte;
        if (++dev->addr_bytes == 3)
            dev->state = W25Q_FAST_READ_DUMMY;
        return 0xFF;

    case W25Q_FAST_READ_DUMMY:
        dev->state = W25Q_FAST_READ_DATA;
        return 0xFF;

    case W25Q_FAST_READ_DATA: {
        uint8_t r = (dev->addr < W25Q128_SIZE) ? dev->data[dev->addr] : 0xFF;
        dev->addr++;
        return r;
    }

    case W25Q_PROGRAM_ADDR:
        dev->addr = (dev->addr << 8) | byte;
        if (++dev->addr_bytes == 3) {
            dev->state = W25Q_PROGRAM_DATA;
            dev->page_addr = dev->addr;
            dev->page_len = 0;
        }
        return 0xFF;

    case W25Q_PROGRAM_DATA:
        if (dev->page_len < W25Q128_PAGE_SIZE)
            dev->page_buf[dev->page_len++] = byte;
        return 0xFF;

    case W25Q_ERASE_ADDR:
        dev->addr = (dev->addr << 8) | byte;
        if (++dev->addr_bytes == 3) {
            commit_sector_erase(dev);
            dev->state = W25Q_IDLE;
        }
        return 0xFF;
    }

    return 0xFF;
}
