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
#include <stdio.h>
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

/* Called when DC pin changes (via GPIO line) */
void ili9341_set_dc(void *opaque, int level)
{
    struct ili9341 *d = (struct ili9341 *)opaque;
    d->dc = level;
    if (!level) {
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

static void write_pixel(struct ili9341 *d, uint16_t pixel)
{
    int r = d->cur_row, c = d->cur_col;
    int ew = ili9341_eff_w(d), eh = ili9341_eff_h(d);
    if (r < eh && c < ew) {
        /* Map logical (col, row) to physical framebuffer position */
        int px, py;
        if (d->madctl & 0x20) { /* MV: swap x/y */
            px = r; py = c;
        } else {
            px = c; py = r;
        }
        if (py < ILI9341_H && px < ILI9341_W)
            d->fb[py * ILI9341_W + px] = pixel;
    }
    advance_cursor(d);
}

static void handle_param(struct ili9341 *d, uint8_t byte)
{
    int ew = ili9341_eff_w(d), eh = ili9341_eff_h(d);
    switch (d->cmd) {
    case 0x2A: /* Column Address Set */
        d->params[d->param_idx++] = byte;
        if (d->param_idx == 4) {
            d->col_start = (d->params[0] << 8) | d->params[1];
            d->col_end   = (d->params[2] << 8) | d->params[3];
            if (d->col_end >= ew) d->col_end = ew - 1;
            d->cur_col = d->col_start;
        }
        break;
    case 0x2B: /* Row Address Set */
        d->params[d->param_idx++] = byte;
        if (d->param_idx == 4) {
            d->row_start = (d->params[0] << 8) | d->params[1];
            d->row_end   = (d->params[2] << 8) | d->params[3];
            if (d->row_end >= eh) d->row_end = eh - 1;
            d->cur_row = d->row_start;
        }
        break;
    case 0x2C: /* Memory Write */
        if (d->pixel_hi) {
            d->hi_byte = byte;
            d->pixel_hi = 0;
        } else {
            write_pixel(d, (d->hi_byte << 8) | byte);
            d->pixel_hi = 1;
            d->dirty = 1;
        }
        break;
    case 0x36: /* Memory Access Control */
        d->madctl = byte;
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
        if (byte == 0x00) ili9341_flush(d); /* NOP = vsync */
    } else {
        /* Data byte — parameter or pixel */
        handle_param(d, byte);
    }
    return 0;
}

void ili9341_flush(struct ili9341 *d)
{
    if (!d->dirty || !d->chardev) return;

    int ew = ili9341_eff_w(d), eh = ili9341_eff_h(d);
    uint8_t hdr[4] = { ew & 0xFF, ew >> 8, eh & 0xFF, eh >> 8 };
    chardev_write_buf(d->chardev, hdr, 4);

    if (d->madctl & 0x20) {
        /* Landscape: transpose physical (240×320) to logical (320×240) */
        static uint16_t tbuf[ILI9341_W * ILI9341_H];
        for (int y = 0; y < eh; y++)
            for (int x = 0; x < ew; x++)
                tbuf[y * ew + x] = d->fb[x * ILI9341_W + y];
        chardev_write_buf(d->chardev, (const uint8_t *)tbuf, ew * eh * 2);
    } else {
        chardev_write_buf(d->chardev, (const uint8_t *)d->fb, ILI9341_W * ILI9341_H * 2);
    }
    d->dirty = 0;
}
