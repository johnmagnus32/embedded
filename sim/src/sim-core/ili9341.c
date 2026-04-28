/*
 * ili9341.c — ILI9341 SPI display controller emulation
 *
 * Implements the subset of commands needed for basic drawing:
 *   0x2A Column Address Set, 0x2B Row Address Set, 0x2C Memory Write
 *   0x36 Memory Access Control, 0x11 Sleep Out, 0x29 Display On
 *
 * Firmware sends commands (DC=0) and data (DC=1) via SPI.
 * Pixel data is RGB565 (2 bytes per pixel), written into a framebuffer.
 */
#include <string.h>
#include "ili9341.h"
#include "chardev.h"

void ili9341_init(struct ili9341 *d)
{
    memset(d, 0, sizeof(*d));
    d->col_end = ILI9341_W - 1;
    d->row_end = ILI9341_H - 1;
    d->dirty = 1;
}

/* Called when DC pin changes (via GPIO write) */
void ili9341_set_dc(void *dev, int active)
{
    struct ili9341 *d = (struct ili9341 *)dev;
    d->dc = active;
    if (!active) {
        /* Entering command mode — reset param state */
        d->param_idx = 0;
        d->pixel_hi = 1;
    }
}

static void advance_cursor(struct ili9341 *d)
{
    d->cur_col++;
    if (d->cur_col > d->col_end) {
        d->cur_col = d->col_start;
        d->cur_row++;
        if (d->cur_row > d->row_end)
            d->cur_row = d->row_start;
    }
}

static void handle_param(struct ili9341 *d, uint8_t byte)
{
    switch (d->cmd) {
    case 0x2A: /* Column Address Set: SC_hi, SC_lo, EC_hi, EC_lo */
        d->params[d->param_idx++] = byte;
        if (d->param_idx == 4) {
            d->col_start = (d->params[0] << 8) | d->params[1];
            d->col_end   = (d->params[2] << 8) | d->params[3];
            if (d->col_end >= ILI9341_W) d->col_end = ILI9341_W - 1;
            d->cur_col = d->col_start;
        }
        break;
    case 0x2B: /* Row Address Set */
        d->params[d->param_idx++] = byte;
        if (d->param_idx == 4) {
            d->row_start = (d->params[0] << 8) | d->params[1];
            d->row_end   = (d->params[2] << 8) | d->params[3];
            if (d->row_end >= ILI9341_H) d->row_end = ILI9341_H - 1;
            d->cur_row = d->row_start;
        }
        break;
    case 0x2C: /* Memory Write — pixel data */
        if (d->pixel_hi) {
            d->hi_byte = byte;
            d->pixel_hi = 0;
        } else {
            uint16_t pixel = (d->hi_byte << 8) | byte;
            if (d->cur_row < ILI9341_H && d->cur_col < ILI9341_W)
                d->fb[d->cur_row * ILI9341_W + d->cur_col] = pixel;
            advance_cursor(d);
            d->pixel_hi = 1;
            d->dirty = 1;
        }
        break;
    case 0x36: /* Memory Access Control */
        /* TODO: handle rotation/mirror bits */
        break;
    }
}

uint8_t ili9341_transfer(void *dev, uint8_t byte)
{
    struct ili9341 *d = (struct ili9341 *)dev;
    if (!d->dc) {
        /* Command byte */
        d->cmd = byte;
        d->param_idx = 0;
        d->pixel_hi = 1;
    } else {
        /* Data byte — parameter or pixel */
        handle_param(d, byte);
    }
    return 0;
}

void ili9341_flush(struct ili9341 *d)
{
    if (!d->dirty || !d->chardev) return;
    chardev_write_buf(d->chardev, (const uint8_t *)d->fb, ILI9341_W * ILI9341_H * 2);
    d->dirty = 0;
}
