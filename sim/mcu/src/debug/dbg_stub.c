/*
 * dbg_stub.c — GDB Remote Serial Protocol stub for sim-core
 *
 * Implements the GDB RSP so standard GDB and VS Code can connect directly.
 * Breakpoints via BKPT instruction patching (0xBE00).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include "dbg_stub.h"
#include "armv7m_cpu.h"
#include "membus.h"
#include "chardev.h"
#include "machine.h"

#define LOG(fmt, ...) fprintf(stderr, "[sim-core] " fmt "\n", ##__VA_ARGS__)

/* --- RSP packet layer --- */

static void gdb_send(int fd, const char *data)
{
    uint8_t sum = 0;
    for (const char *p = data; *p; p++) sum += (uint8_t)*p;
    char buf[16384];
    int n = snprintf(buf, sizeof(buf), "$%s#%02x", data, sum);
    write(fd, buf, n);
}

static int gdb_recv(int fd, char *buf, int bufsize)
{
    char c;
    /* Skip until $ */
    do { if (read(fd, &c, 1) <= 0) return -1; } while (c != '$');
    /* Read until # */
    int len = 0;
    while (len < bufsize - 1) {
        if (read(fd, &c, 1) <= 0) return -1;
        if (c == '#') break;
        buf[len++] = c;
    }
    buf[len] = '\0';
    /* Consume 2-byte checksum */
    char ck[2];
    read(fd, ck, 2);
    /* ACK */
    write(fd, "+", 1);
    return len;
}

/* --- Breakpoint table (BKPT patching) --- */

#define MAX_BP 32
#define BKPT_INSN 0xBE00

struct breakpoint {
    uint32_t addr;
    uint16_t orig_insn;
    int      active;
};

static struct breakpoint bps[MAX_BP];
static int nbp;

static int bp_find(uint32_t addr)
{
    for (int i = 0; i < nbp; i++)
        if (bps[i].addr == addr) return i;
    return -1;
}

static void bp_patch(uint8_t *flash, int idx)
{
    uint32_t off = bps[idx].addr - FLASH_BASE;
    bps[idx].orig_insn = flash[off] | (flash[off + 1] << 8);
    flash[off] = BKPT_INSN & 0xFF;
    flash[off + 1] = BKPT_INSN >> 8;
    bps[idx].active = 1;
}

static void bp_unpatch(uint8_t *flash, int idx)
{
    uint32_t off = bps[idx].addr - FLASH_BASE;
    flash[off] = bps[idx].orig_insn & 0xFF;
    flash[off + 1] = bps[idx].orig_insn >> 8;
    bps[idx].active = 0;
}

static void bp_unpatch_all(uint8_t *flash)
{
    for (int i = 0; i < nbp; i++)
        if (bps[i].active) bp_unpatch(flash, i);
}

/* Step one instruction, temporarily unpatching breakpoint at PC if needed */
static void single_step(struct stub_ctx *ctx)
{
    uint32_t pc = ctx->cpu->r[REG_PC];
    int bp_idx = bp_find(pc);
    if (bp_idx >= 0 && bps[bp_idx].active)
        bp_unpatch(*ctx->flash, bp_idx);
    ctx->mach->tick(ctx->board);
    if (bp_idx >= 0 && !bps[bp_idx].active)
        bp_patch(*ctx->flash, bp_idx);
}

/* Forward declaration — needed for async packet handling during continue */
static void dispatch(int fd, struct stub_ctx *ctx, const char *pkt);

/* Continue execution. Returns: 0=breakpoint, 1=halt(Ctrl+C), 2=semihost exit */
static int run_continue(int fd, struct stub_ctx *ctx)
{
    int bp_at_pc = bp_find(ctx->cpu->r[REG_PC]);
    if (bp_at_pc >= 0) single_step(ctx);

    int tick_count = 0;
    while (1) {
        int r = ctx->mach->tick(ctx->board);
        if (r & CPU_SEMIHOST_EXIT) return 2;
        if (r & CPU_BREAKPOINT) return 0;

        if (++tick_count >= 10000) {
            tick_count = 0;
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(fd, &fds);
            struct timeval tv = {0, 0};
            if (select(fd + 1, &fds, NULL, NULL, &tv) > 0) {
                char c;
                if (recv(fd, &c, 1, MSG_PEEK) > 0 && c == 0x03) {
                    read(fd, &c, 1);
                    return 1;  /* Ctrl+C halt */
                }
                /* Real packet — handle without stopping */
                char pkt[256];
                if (gdb_recv(fd, pkt, sizeof(pkt)) > 0)
                    dispatch(fd, ctx, pkt);
            }
        }
    }
}

/* Flush chardevs so output is visible after stop */
static void flush_on_stop(struct stub_ctx *ctx)
{
    if (ctx->chardevs)
        chardev_flush_all(ctx->chardevs);
}

/* Format a 32-bit value as little-endian hex (8 chars) */
static int hex32(char *buf, uint32_t v)
{
    return sprintf(buf, "%02x%02x%02x%02x",
                   v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF, (v >> 24) & 0xFF);
}

/* Parse a little-endian hex 32-bit value */
static uint32_t parse_hex32(const char *p)
{
    uint32_t v = 0;
    for (int b = 0; b < 4; b++) {
        unsigned int byte;
        sscanf(p + b * 2, "%2x", &byte);
        v |= byte << (b * 8);
    }
    return v;
}

/* --- Packet dispatch --- */

static void dispatch(int fd, struct stub_ctx *ctx, const char *pkt)
{
    char resp[16384];

    switch (pkt[0]) {

    case '?':
        /* Stop reason */
        flush_on_stop(ctx);
        gdb_send(fd, "S05");
        break;

    case 'g': {
        /* Read registers: r0-r15, 8×FPA(zeros), xPSR */
        int n = 0;
        for (int i = 0; i < 16; i++)
            n += hex32(resp + n, ctx->cpu->r[i]);
        /* FPA registers: 8 × 12 bytes = 96 bytes = 192 hex chars of zeros */
        for (int i = 0; i < 192; i++)
            resp[n++] = '0';
        /* xPSR */
        n += hex32(resp + n, ctx->cpu->xpsr);
        resp[n] = '\0';
        gdb_send(fd, resp);
        break;
    }

    case 'G': {
        /* Write registers */
        const char *p = pkt + 1;
        for (int i = 0; i < 16; i++) {
            ctx->cpu->r[i] = parse_hex32(p);
            p += 8;
        }
        p += 192;  /* skip FPA */
        ctx->cpu->xpsr = parse_hex32(p);
        gdb_send(fd, "OK");
        break;
    }

    case 'm': {
        /* Read memory */
        uint32_t addr; int len;
        sscanf(pkt + 1, "%x,%x", &addr, &len);
        if (len > 4096) len = 4096;
        int n = 0;
        for (int i = 0; i < len; i++)
            n += sprintf(resp + n, "%02x", membus_read8(ctx->bus, addr + i));
        gdb_send(fd, resp);
        break;
    }

    case 'M': {
        /* Write memory */
        uint32_t addr; int len;
        sscanf(pkt + 1, "%x,%x", &addr, &len);
        const char *hex = strchr(pkt, ':');
        if (!hex) { gdb_send(fd, "E01"); break; }
        hex++;
        for (int i = 0; i < len; i++) {
            unsigned int b;
            sscanf(hex + i * 2, "%2x", &b);
            membus_write8(ctx->bus, addr + i, (uint8_t)b);
        }
        gdb_send(fd, "OK");
        break;
    }

    case 's':
        /* Single step */
        single_step(ctx);
        flush_on_stop(ctx);
        gdb_send(fd, "S05");
        break;

    case 'c': {
        /* Continue */
        int reason = run_continue(fd, ctx);
        flush_on_stop(ctx);
        if (reason == 2) {
            if (ctx->chardevs) {
                chardev_flush_all(ctx->chardevs);
                chardev_shutdown_all(ctx->chardevs);
            }
            gdb_send(fd, "W00");
            exit(0);
        }
        gdb_send(fd, "S05");
        break;
    }

    case 'Z':
        /* Set breakpoint */
        if (pkt[1] == '0') {
            uint32_t addr;
            sscanf(pkt + 3, "%x", &addr);
            if (bp_find(addr) < 0 && nbp < MAX_BP) {
                bps[nbp].addr = addr;
                bp_patch(*ctx->flash, nbp);
                nbp++;
            }
            gdb_send(fd, "OK");
        } else {
            gdb_send(fd, "");
        }
        break;

    case 'z':
        /* Remove breakpoint */
        if (pkt[1] == '0') {
            uint32_t addr;
            sscanf(pkt + 3, "%x", &addr);
            int idx = bp_find(addr);
            if (idx >= 0) {
                if (bps[idx].active) bp_unpatch(*ctx->flash, idx);
                bps[idx] = bps[--nbp];
            }
            gdb_send(fd, "OK");
        } else {
            gdb_send(fd, "");
        }
        break;

    case 'q':
        /* Query packets */
        if (strncmp(pkt, "qSupported", 10) == 0) {
            gdb_send(fd, "PacketSize=4096;swbreak+");
        } else if (strncmp(pkt, "qAttached", 9) == 0) {
            gdb_send(fd, "1");
        } else {
            gdb_send(fd, "");
        }
        break;

    case 'H':
        /* Set thread — we're single-threaded, always OK */
        gdb_send(fd, "OK");
        break;

    case 'D':
        /* Detach */
        gdb_send(fd, "OK");
        break;

    default:
        /* Unsupported packet — empty response */
        gdb_send(fd, "");
        break;
    }
}

/* --- Server --- */

void dbg_stub_run(struct stub_ctx *ctx, int port)
{
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(port),
                                .sin_addr.s_addr = htonl(INADDR_ANY) };
    if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG("Failed to bind GDB port %d", port);
        exit(1);
    }
    listen(srv, 1);
    LOG("GDB stub on port %d", port);

    int client = accept(srv, NULL, NULL);
    LOG("GDB client connected");

    /* GDB sends + first, then qSupported. We just start reading packets. */
    char pkt[8192];
    while (1) {
        int len = gdb_recv(client, pkt, sizeof(pkt));
        if (len < 0) break;
        dispatch(client, ctx, pkt);
    }

    bp_unpatch_all(*ctx->flash);
    nbp = 0;
    close(client);
    close(srv);
    LOG("GDB client disconnected");
}
