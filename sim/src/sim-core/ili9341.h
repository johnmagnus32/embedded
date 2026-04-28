#ifndef ILI9341_H
#define ILI9341_H

#include <stdint.h>

#define ILI9341_W 240
#define ILI9341_H 320

struct ili9341 {
    uint16_t fb[ILI9341_W * ILI9341_H]; /* RGB565 framebuffer */
    /* Column/row window */
    uint16_t col_start, col_end, row_start, row_end;
    uint16_t cur_col, cur_row;
    /* Command state */
    uint8_t  cmd;          /* current command being processed */
    int      param_idx;    /* parameter byte index */
    uint8_t  params[4];    /* parameter accumulator */
    int      dc;           /* 0=command, 1=data */
    int      pixel_hi;     /* 1 if next byte is high byte of RGB565 */
    uint8_t  hi_byte;      /* saved high byte */
    int      dirty;        /* framebuffer changed since last read */
};

void    ili9341_init(struct ili9341 *d);
void    ili9341_set_dc(void *dev, int active); /* DC pin: 0=cmd, 1=data */
uint8_t ili9341_transfer(void *dev, uint8_t byte);

#endif
