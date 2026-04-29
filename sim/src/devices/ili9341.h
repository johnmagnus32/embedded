#ifndef ILI9341_DEV_H
#define ILI9341_DEV_H

#include <stdint.h>

struct chardev;

#define ILI9341_W 240
#define ILI9341_H 320

struct ili9341 {
    uint16_t fb[ILI9341_W * ILI9341_H]; /* RGB565 framebuffer */
    /* Column/row window */
    uint16_t col_start, col_end, row_start, row_end;
    uint16_t cur_col, cur_row;
    /* Command state */
    uint8_t  cmd;
    int      param_idx;
    uint8_t  params[4];
    int      dc;
    int      pixel_hi;
    uint8_t  hi_byte;
    int      dirty;
    uint8_t  madctl;       /* Memory Access Control register */
    struct chardev *chardev;
};

/* Effective dimensions based on MADCTL MV bit */
static inline int ili9341_eff_w(struct ili9341 *d) { return (d->madctl & 0x20) ? ILI9341_H : ILI9341_W; }
static inline int ili9341_eff_h(struct ili9341 *d) { return (d->madctl & 0x20) ? ILI9341_W : ILI9341_H; }

void    ili9341_init(struct ili9341 *d);
void    ili9341_set_dc(void *opaque, int level); /* gpio_handler_fn: DC pin */
uint8_t ili9341_transfer(void *dev, uint8_t byte);
void    ili9341_flush(struct ili9341 *d);      /* push framebuffer to chardev if dirty */

#endif
