/*
 * trace.h — Lightweight tracing via memory-mapped trace port
 *
 * Writes trace events to a fixed address. The emulator captures
 * these and streams them to the debugger for timeline display.
 *
 * Usage:
 *   trace_begin("task_a");   // mark start of a named span
 *   trace_end();             // mark end of current span
 *   trace_event("isr:tick"); // instantaneous event
 */
#ifndef TRACE_H
#define TRACE_H

/* Trace port: write a null-terminated string here to emit a trace event.
 * Uses an unused ITM stimulus port address. */
#define TRACE_PORT ((volatile char *)0xE0000000)

static inline void trace_begin(const char *name)
{
    /* Write "B:name" — B = begin */
    *TRACE_PORT = 'B';
    *TRACE_PORT = ':';
    while (*name) *TRACE_PORT = *name++;
    *TRACE_PORT = '\n';
}

static inline void trace_end(void)
{
    *TRACE_PORT = 'E';
    *TRACE_PORT = '\n';
}

static inline void trace_event(const char *name)
{
    *TRACE_PORT = 'I';
    *TRACE_PORT = ':';
    while (*name) *TRACE_PORT = *name++;
    *TRACE_PORT = '\n';
}

/* Heap tracing: H:name@addr,size  F:addr */
static inline void trace_alloc(const char *name, void *ptr, unsigned size)
{
    *TRACE_PORT = 'H';
    *TRACE_PORT = ':';
    while (*name) *TRACE_PORT = *name++;
    *TRACE_PORT = '@';
    /* Emit addr as hex */
    unsigned a = (unsigned)ptr;
    for (int i = 28; i >= 0; i -= 4)
        *TRACE_PORT = "0123456789abcdef"[(a >> i) & 0xf];
    *TRACE_PORT = ',';
    /* Emit size as hex */
    for (int i = 28; i >= 0; i -= 4)
        *TRACE_PORT = "0123456789abcdef"[(size >> i) & 0xf];
    *TRACE_PORT = '\n';
}

static inline void trace_free(void *ptr)
{
    *TRACE_PORT = 'F';
    *TRACE_PORT = ':';
    unsigned a = (unsigned)ptr;
    for (int i = 28; i >= 0; i -= 4)
        *TRACE_PORT = "0123456789abcdef"[(a >> i) & 0xf];
    *TRACE_PORT = '\n';
}

#endif
