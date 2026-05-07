/*
 * trace_dev.c — Trace port device
 *
 * Buffers characters until newline, then sends the complete line
 * with cycle count appended in a single chardev_write_buf call.
 */
#include <stdio.h>
#include <string.h>
#include "trace_dev.h"
#include "chardev.h"

void trace_dev_init(struct trace_dev *t, struct chardev *cd, uint64_t *cycle_count)
{
    t->chardev = cd;
    t->cycle_count = cycle_count;
    t->buf_len = 0;
}

uint32_t trace_dev_read(void *opaque, uint32_t offset)
{
    (void)opaque; (void)offset;
    return 0;
}

void trace_dev_write(void *opaque, uint32_t offset, uint32_t val)
{
    struct trace_dev *t = (struct trace_dev *)opaque;
    (void)offset;
    if (!t->chardev) return;

    char c = (char)(val & 0xFF);
    if (c == '\n') {
        /* Append @cycle\n and send entire line */
        uint64_t cy = t->cycle_count ? *t->cycle_count : 0;
        int n = snprintf(t->buf + t->buf_len, sizeof(t->buf) - t->buf_len,
                         "@%lu\n", (unsigned long)cy);
        t->buf_len += n;
        chardev_write_buf(t->chardev, (const uint8_t *)t->buf, t->buf_len);
        t->buf_len = 0;
    } else if (t->buf_len < (int)sizeof(t->buf) - 32) {
        t->buf[t->buf_len++] = c;
    }
}
