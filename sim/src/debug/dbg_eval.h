#ifndef DBG_EVAL_H
#define DBG_EVAL_H

#include <stdint.h>

struct armv7m_cpu;

int dbg_eval(const char *expr, struct armv7m_cpu *cpu, uint8_t *flash,
             uint8_t *ram, char *buf, int bufsize);

#endif
