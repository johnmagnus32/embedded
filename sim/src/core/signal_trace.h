#ifndef SIGNAL_TRACE_H
#define SIGNAL_TRACE_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct signal_trace {
    FILE *fp;
    uint64_t *cycle_ptr;
};

/* Create a trace. Returns NULL if path is NULL (zero cost). */
struct signal_trace *signal_trace_create(const char *path, uint64_t *cycle_ptr);
void signal_trace_close(struct signal_trace *t);

static inline void signal_trace_byte(struct signal_trace *t, uint8_t byte)
{
    if (!t) return;
    fprintf(t->fp, "%llu,data,0x%02X\n", (unsigned long long)*t->cycle_ptr, byte);
}

static inline void signal_trace_event(struct signal_trace *t, const char *event)
{
    if (!t) return;
    fprintf(t->fp, "%llu,event,%s\n", (unsigned long long)*t->cycle_ptr, event);
}

/* Trace table: parsed from --trace name=path args */
#define MAX_TRACES 16

struct trace_table {
    struct { char name[32]; char path[256]; } entries[MAX_TRACES];
    int count;
};

static inline const char *trace_find(struct trace_table *t, const char *name)
{
    if (!t) return NULL;
    for (int i = 0; i < t->count; i++)
        if (strcmp(t->entries[i].name, name) == 0)
            return t->entries[i].path;
    return NULL;
}

#endif
