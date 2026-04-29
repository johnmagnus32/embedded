#ifndef DBG_TASKS_H
#define DBG_TASKS_H

#include <stdint.h>

struct armv7m_cpu;

int dbg_emit_tasks(struct armv7m_cpu *cpu, uint8_t *flash, uint8_t *ram,
                   char *buf, int bufsize);

#endif
