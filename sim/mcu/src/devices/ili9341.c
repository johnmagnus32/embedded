/*
 * ili9341.c — ILI9341 SPI display controller emulation
 *
 * Double-buffered frame output:
 *   ili9341_flush() writes a complete frame into the write slot.
 *   ili9341_send() drains the ready slot to the chardev TCP socket.
 *   Frames are never partially overwritten — every frame the receiver
 *   gets is complete.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include "ili9341.h"
#include "chardev.h"

void ili9341_init(struct ili9341 *d)
{
    memset(d, 0, sizeof(*d));
    d->col_end = ILI9341_W - 1;
    d->row_end = ILI9341_H - 1;
    d->refresh_interval = 16000000 / ILI9341_REFRESH_HZ;
    d->next_refresh = 0;
    d->send_ready = 0;
    d->send_write = 1;
}

void ili9341_set_dc(void *opaque, int level)
{
    struct ili9341 *d = (struct ili9341 *)opaque;
    d->dc = level;
    if (!level) { d->param_idx = 0; d->pixel_hi = 1; }
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
        /* Always store in row-major output order so flush is a straight memcpy */
        d->fb[r * ew + c] = pixel;
    }
    advance_cursor(d);
}

static void handle_param(struct ili9341 *d, uint8_t byte)
{
    int ew = ili9341_eff_w(d), eh = ili9341_eff_h(d);
    switch (d->cmd) {
    case 0x2A:
        d->params[d->param_idx++] = byte;
        if (d->param_idx == 4) {
            d->col_start = (d->params[0] << 8) | d->params[1];
            d->col_end   = (d->params[2] << 8) | d->params[3];
            if (d->col_end >= ew) d->col_end = ew - 1;
            d->cur_col = d->col_start;
        }
        break;
    case 0x2B:
        d->params[d->param_idx++] = byte;
        if (d->param_idx == 4) {
            d->row_start = (d->params[0] << 8) | d->params[1];
            d->row_end   = (d->params[2] << 8) | d->params[3];
            if (d->row_end >= eh) d->row_end = eh - 1;
            d->cur_row = d->row_start;
        }
        break;
    case 0x2C:
        if (d->pixel_hi) { d->hi_byte = byte; d->pixel_hi = 0; }
        else { write_pixel(d, (d->hi_byte << 8) | byte); d->pixel_hi = 1; }
        break;
    case 0x36:
        d->madctl = byte;
        break;
    }
}

uint8_t ili9341_transfer(void *dev, uint8_t byte)
{
    struct ili9341 *d = (struct ili9341 *)dev;
    if (!d->dc) { d->cmd = byte; d->param_idx = 0; d->pixel_hi = 1; }
    else handle_param(d, byte);
    return 0;
}

void ili9341_flush(struct ili9341 *d)
{
    if (!d->chardev) return;

    int ew = ili9341_eff_w(d), eh = ili9341_eff_h(d);
    uint8_t *out = d->send_frames[d->send_write];
    int pos = 0;

    /* Header */
    out[pos++] = ew & 0xFF;
    out[pos++] = ew >> 8;
    out[pos++] = eh & 0xFF;
    out[pos++] = eh >> 8;

    /* Pixel data — already in row-major output order from write_pixel */
    int frame_bytes = ew * eh * 2;
    memcpy(out + pos, d->fb, frame_bytes);
    pos += frame_bytes;

    /* Swap: make this frame the ready frame */
    int old_write = d->send_write;
    d->send_write = d->send_ready;
    d->send_ready = old_write;
    d->send_size = pos;
    d->send_offset = 0;
    d->has_ready_frame = 1;
}

void ili9341_send(struct ili9341 *d)
{
    if (!d->has_ready_frame || !d->chardev) return;

    /* Ensure client is connected */
    if (d->chardev->client_fd < 0) {
        chardev_try_accept(d->chardev);
        if (d->chardev->client_fd < 0) return;
    }

    /* Send as much as TCP will accept */
    while (d->send_offset < d->send_size) {
        int remaining = d->send_size - d->send_offset;
        int n = write(d->chardev->client_fd,
                      d->send_frames[d->send_ready] + d->send_offset,
                      remaining);
        if (n > 0) {
            d->send_offset += n;
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return;  /* socket full, try again next flush */
        /* Error — client disconnected */
        d->chardev->client_fd = -1;
        return;
    }

    /* Entire frame sent */
    d->has_ready_frame = 0;
}

void ili9341_tick(struct ili9341 *d)
{
    if (__builtin_expect(d->cycle_count_ptr &&
                         *d->cycle_count_ptr >= d->next_refresh, 0)) {
        ili9341_flush(d);
        d->next_refresh = *d->cycle_count_ptr + d->refresh_interval;
    }
}
