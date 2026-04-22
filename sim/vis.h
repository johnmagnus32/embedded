/*
 * vis.h — CPU/memory state visualizer
 *
 * Prints an ASCII snapshot of:
 *   - Memory map (flash, RAM regions, stacks, heap)
 *   - Register state (PC, SP, LR, flags)
 *   - Task list with active task highlighted
 *   - Stack pointer arrows showing where each task's SP is
 */

#ifndef VIS_H
#define VIS_H

#include "cpu.h"

/* Dump a snapshot to the given file (stderr or a log file) */
void vis_dump(FILE *out, struct cpu_state *cpu, uint8_t *flash, uint8_t *ram);

#endif
