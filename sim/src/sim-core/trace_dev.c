/*
 * trace_dev.c — Trace port device
 *
 * Captures byte writes to the trace port address and streams them
 * to a chardev. The firmware writes trace events as strings
 * ("B:task_a\n", "E\n", "I:isr\n") and the debugger UI parses them.
 *
 * Also prepends a cycle count to each line for timeline rendering.
 */
#include <stdio.h>
#include "trace_dev.h"
#include "chardev.h"
#include "board.h"

/* We need the cycle count — get it from the global board */
extern struct board *g_board;

void trace_dev_init(struct trace_dev *t, uint32_t base, struct chardev *cd)
{
    t->base = base;
    t->chardev = cd;
}

int trace_dev_handles(struct trace_dev *t, uint32_t addr)
{
    return addr == t->base;
}

uint32_t trace_dev_read(struct trace_dev *t, uint32_t addr)
{
    (void)t; (void)addr;
    return 0;
}

void trace_dev_write(struct trace_dev *t, uint32_t addr, uint32_t val)
{
    (void)addr;
    if (!t->chardev) return;

    char c = (char)(val & 0xFF);

    /* On newline, prepend cycle count */
    if (c == '\n') {
        char buf[32];
        extern struct board *g_board;
        uint64_t cy = g_board ? g_board->cpu.cycle_count : 0;
        int n = snprintf(buf, sizeof(buf), "@%lu\n", (unsigned long)cy);
        for (int i = 0; i < n; i++)
            chardev_write(t->chardev, (uint8_t)buf[i]);
    } else {
        chardev_write(t->chardev, (uint8_t)c);
    }
}
