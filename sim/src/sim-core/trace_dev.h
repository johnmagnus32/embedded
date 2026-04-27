#ifndef TRACE_DEV_H
#define TRACE_DEV_H

#include <stdint.h>

struct chardev;

struct trace_dev {
    uint32_t base;
    struct chardev *chardev;
};

void     trace_dev_init(struct trace_dev *t, uint32_t base, struct chardev *cd);
int      trace_dev_handles(struct trace_dev *t, uint32_t addr);
uint32_t trace_dev_read(struct trace_dev *t, uint32_t addr);
void     trace_dev_write(struct trace_dev *t, uint32_t addr, uint32_t val);

#endif
