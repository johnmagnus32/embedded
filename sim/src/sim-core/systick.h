#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdint.h>

struct nvic;

struct systick {
    uint32_t csr;
    uint32_t rvr;
    uint32_t cvr;
    uint64_t counter;
};

void     systick_init(struct systick *st);
void     systick_tick(struct systick *st, struct nvic *nvic);
int      systick_handles(uint32_t addr);
uint32_t systick_read(struct systick *st, uint32_t addr);
void     systick_write(struct systick *st, uint32_t addr, uint32_t val);

#endif
