#ifndef DBG_SERVER_H
#define DBG_SERVER_H

#include "machine.h"
#include "armv7m_cpu.h"
#include "membus.h"

struct sim_ctx {
    const struct machine_desc *mach;
    void *board;
    struct armv7m_cpu *cpu;
    struct membus *bus;
    uint8_t **flash;
    uint8_t **ram;
};

void dbg_server_run(struct sim_ctx *ctx, int port);
void send_response(int fd, const char *json);

#endif
