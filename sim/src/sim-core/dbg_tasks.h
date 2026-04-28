#ifndef DBG_TASKS_H
#define DBG_TASKS_H

#include <stdint.h>

struct cpu_state;

int dbg_emit_tasks(struct cpu_state *cpu, uint8_t *flash, uint8_t *ram,
                   char *buf, int bufsize);

#endif
