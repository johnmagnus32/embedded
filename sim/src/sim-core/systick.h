#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdint.h>

struct nvic;

struct systick {
    uint32_t csr;       /* Control and Status Register */
    uint32_t rvr;       /* Reload Value Register */
    uint32_t cvr;       /* Current Value Register */
    uint64_t counter;   /* internal cycle counter */
};

void     systick_init(struct systick *st);
void     systick_tick(struct systick *st, struct nvic *nvic);

/* Memory-mapped register access (0xE000E010 range) */
uint32_t systick_read(struct systick *st, uint32_t addr);
void     systick_write(struct systick *st, uint32_t addr, uint32_t val);
int      systick_handles(uint32_t addr);

#endif
