#ifndef ILI9341_H
#define ILI9341_H

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
    struct chardev *chardev;  /* for framebuffer streaming */
};

void    ili9341_init(struct ili9341 *d);
void    ili9341_set_dc(void *dev, int active); /* DC pin: 0=cmd, 1=data */
uint8_t ili9341_transfer(void *dev, uint8_t byte);

#endif
