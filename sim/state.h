#ifndef STATE_H
#define STATE_H
#include "cpu.h"
void state_set_path(const char *path);
void state_dump(struct cpu_state *cpu, uint8_t *flash, uint8_t *ram);
#endif
