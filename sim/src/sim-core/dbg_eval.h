#ifndef DBG_EVAL_H
#define DBG_EVAL_H

#include <stdint.h>

struct cpu_state;

int dbg_eval(const char *expr, struct cpu_state *cpu, uint8_t *flash,
             uint8_t *ram, char *buf, int bufsize);

#endif
