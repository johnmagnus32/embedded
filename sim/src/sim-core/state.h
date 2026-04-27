#ifndef STATE_H
#define STATE_H
#include <stdio.h>
#include "cpu.h"
void state_set_path(const char *path);
void state_set_source_dir(const char *dir);
void state_dump(struct cpu_state *cpu, uint8_t *flash, uint8_t *ram);
void state_dump_to(struct cpu_state *cpu, uint8_t *flash, uint8_t *ram, FILE *out);
void timeline_record(uint64_t cycle, const char *ctx);
void timeline_dump(FILE *f);
#endif
