#ifndef TRACE_DEV_H
#define TRACE_DEV_H

#include <stdint.h>

struct chardev;

struct trace_dev {
    struct chardev *chardev;
    uint64_t *cycle_count;
};

void     trace_dev_init(struct trace_dev *t, struct chardev *cd, uint64_t *cycle_count);
uint32_t trace_dev_read(void *opaque, uint32_t offset);
void     trace_dev_write(void *opaque, uint32_t offset, uint32_t val);

#endif
