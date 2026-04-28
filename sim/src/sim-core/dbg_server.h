#ifndef DBG_SERVER_H
#define DBG_SERVER_H

#include "machine.h"
#include "cpu.h"
#include "membus.h"
#include "armv7m_nvic.h"

struct sim_ctx {
    const struct machine_desc *mach;
    void *board;
    struct cpu_state *cpu;
    struct membus *bus;
    uint8_t **flash;
    uint8_t **ram;
    struct armv7m_nvic *nvic;
};

void dbg_server_run(struct sim_ctx *ctx, int port);
void send_response(int fd, const char *json);

#endif
