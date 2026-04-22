#ifndef VIS_H
#define VIS_H

#include <stdio.h>
#include "cpu.h"

void vis_dump(FILE *out, struct cpu_state *cpu, uint8_t *flash, uint8_t *ram,
              const char *event);
void vis_dbg_log(const char *fmt, ...);

#endif
