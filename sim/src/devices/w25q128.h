#ifndef W25Q128_H
#define W25Q128_H

#include <stdint.h>
#include <stddef.h>

struct spi_slave;

#define W25Q128_SIZE       (16 * 1024 * 1024)  /* 16 MB */
#define W25Q128_PAGE_SIZE  256
#define W25Q128_SECTOR_SIZE 4096

/* JEDEC ID */
#define W25Q128_MFR_ID     0xEF
#define W25Q128_DEV_TYPE   0x40
#define W25Q128_DEV_CAP    0x18

/* Commands */
#define W25Q_CMD_WRITE_ENABLE   0x06
#define W25Q_CMD_WRITE_DISABLE  0x04
#define W25Q_CMD_READ_STATUS    0x05
#define W25Q_CMD_READ_DATA      0x03
#define W25Q_CMD_FAST_READ      0x0B
#define W25Q_CMD_PAGE_PROGRAM   0x02
#define W25Q_CMD_SECTOR_ERASE   0x20
#define W25Q_CMD_JEDEC_ID       0x9F

/* Status register bits */
#define W25Q_STATUS_BUSY  0x01
#define W25Q_STATUS_WEL   0x02

enum w25q_state {
    W25Q_IDLE,
    W25Q_READ_ADDR,
    W25Q_READ_DATA,
    W25Q_FAST_READ_ADDR,
    W25Q_FAST_READ_DUMMY,
    W25Q_FAST_READ_DATA,
    W25Q_PROGRAM_ADDR,
    W25Q_PROGRAM_DATA,
    W25Q_ERASE_ADDR,
    W25Q_JEDEC_ID,
    W25Q_READ_STATUS,
};

struct w25q128 {
    uint8_t *data;              /* 16MB backing store */
    enum w25q_state state;
    uint32_t addr;              /* current address being assembled/used */
    int addr_bytes;             /* bytes of address received so far */
    uint8_t status;             /* status register (BUSY | WEL) */
    uint8_t cmd;                /* current command */
    int jedec_idx;              /* byte index for JEDEC ID response */
    /* Page program buffer */
    uint8_t page_buf[W25Q128_PAGE_SIZE];
    uint32_t page_addr;         /* base address of page being programmed */
    int page_len;               /* bytes written to page_buf */
    /* Busy timing */
    uint64_t *cycle_count_ptr;  /* pointer to CPU cycle counter */
    uint64_t busy_until;        /* cycle count when busy clears */
    /* SPI bus slave entry (for cs_active toggling) */
    struct spi_slave *spi_slave;
};

void    w25q128_init(struct w25q128 *dev);
void    w25q128_free(struct w25q128 *dev);
int     w25q128_load(struct w25q128 *dev, const char *path);
uint8_t w25q128_transfer(void *dev, uint8_t byte);
void    w25q128_cs_deassert(struct w25q128 *dev);
void    w25q128_cs_handler(void *opaque, int level);

#endif
