/*
 * dbg_stub.h — Thin debug stub for sim-core
 *
 * Exposes raw CPU state over JSON/TCP. No symbols, no DWARF.
 * Breakpoints via BKPT instruction patching (0xBE00).
 */
#ifndef DBG_STUB_H
#define DBG_STUB_H

#include <stdint.h>

struct armv7m_cpu;
struct membus;
struct machine_desc;
struct chardev_table;

struct stub_ctx {
    const struct machine_desc *mach;
    void *board;
    struct armv7m_cpu *cpu;
    struct membus *bus;
    uint8_t **flash;
    uint8_t **ram;
    struct chardev_table *chardevs;
};

/*
 * Run the debug stub server. Binds to port, accepts one client,
 * processes commands until client disconnects.
 */
void dbg_stub_run(struct stub_ctx *ctx, int port);

#endif
