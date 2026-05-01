#ifndef ILI9341_DEV_H
#define ILI9341_DEV_H

#include <stdint.h>

struct chardev;

#define ILI9341_W 240
#define ILI9341_H 320
#define ILI9341_REFRESH_HZ 60

struct ili9341 {
    uint16_t fb[ILI9341_W * ILI9341_H];
    uint16_t col_start, col_end, row_start, row_end;
    uint16_t cur_col, cur_row;
    uint8_t  cmd;
    int      param_idx;
    uint8_t  params[4];
    int      dc;
    int      pixel_hi;
    uint8_t  hi_byte;
    uint8_t  madctl;
    struct chardev *chardev;
    uint32_t refresh_interval;
    uint64_t next_refresh;       /* cycle count when next refresh fires */
    uint64_t *cycle_count_ptr;   /* pointer to cpu->cycle_count */
};

static inline int ili9341_eff_w(struct ili9341 *d) { return (d->madctl & 0x20) ? ILI9341_H : ILI9341_W; }
static inline int ili9341_eff_h(struct ili9341 *d) { return (d->madctl & 0x20) ? ILI9341_W : ILI9341_H; }

void    ili9341_init(struct ili9341 *d);
void    ili9341_set_dc(void *opaque, int level);
uint8_t ili9341_transfer(void *dev, uint8_t byte);
void    ili9341_tick(struct ili9341 *d);
void    ili9341_flush(struct ili9341 *d);

#endif
