#ifndef ILI9341_DEV_H
#define ILI9341_DEV_H

#include <stdint.h>

struct chardev;

#define ILI9341_W 240
#define ILI9341_H 320
#define ILI9341_REFRESH_HZ 60
#define ILI9341_FRAME_BYTES (ILI9341_W * ILI9341_H * 2 + 4)  /* pixels + 4-byte header */

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
    uint64_t next_refresh;
    uint64_t *cycle_count_ptr;

    /* Double-buffered frame output */
    uint8_t  send_frames[2][ILI9341_FRAME_BYTES];
    int      send_ready;       /* index of frame ready to send (0 or 1) */
    int      send_write;       /* index of frame being written (0 or 1) */
    int      send_offset;      /* bytes already sent from ready frame */
    int      send_size;        /* total bytes in ready frame */
    int      has_ready_frame;  /* 1 if there's a frame to send */
};

static inline int ili9341_eff_w(struct ili9341 *d) { return (d->madctl & 0x20) ? ILI9341_H : ILI9341_W; }
static inline int ili9341_eff_h(struct ili9341 *d) { return (d->madctl & 0x20) ? ILI9341_W : ILI9341_H; }

void    ili9341_init(struct ili9341 *d);
void    ili9341_set_dc(void *opaque, int level);
uint8_t ili9341_transfer(void *dev, uint8_t byte);
void    ili9341_tick(struct ili9341 *d);
void    ili9341_flush(struct ili9341 *d);

/* Send buffered frame data over TCP. Call from chardev flush loop. */
void    ili9341_send(struct ili9341 *d);

#endif
