/*
 * trace_dev.c — Trace port device
 */
#include <stdio.h>
#include "trace_dev.h"
#include "chardev.h"

void trace_dev_init(struct trace_dev *t, struct chardev *cd, uint64_t *cycle_count)
{
    t->chardev = cd;
    t->cycle_count = cycle_count;
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
        char buf[32];
        uint64_t cy = t->cycle_count ? *t->cycle_count : 0;
        int n = snprintf(buf, sizeof(buf), "@%lu\n", (unsigned long)cy);
        for (int i = 0; i < n; i++)
            chardev_write(t->chardev, (uint8_t)buf[i]);
    } else {
        chardev_write(t->chardev, (uint8_t)c);
    }
}
