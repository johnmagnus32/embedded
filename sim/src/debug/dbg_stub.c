/*
 * dbg_stub.c — Thin debug stub for sim-core
 *
 * JSON/TCP protocol. Breakpoints via BKPT instruction patching.
 * Checks for incoming commands during continue via non-blocking select().
 * No symbols, no DWARF — just raw CPU state.
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

/* --- JSON helpers --- */

static void send_resp(int fd, const char *json)
{
    write(fd, json, strlen(json));
    write(fd, "\n", 1);
}

static void send_stop(int fd, struct stub_ctx *ctx)
{
    if (ctx->chardevs)
        chardev_flush_all(ctx->chardevs);
    char buf[128];
    snprintf(buf, sizeof(buf),
        "{\"stopped\":true,\"pc\":%u,\"cycles\":%lu}",
        ctx->cpu->r[REG_PC], (unsigned long)ctx->cpu->cycle_count);
    send_resp(fd, buf);
}

static const char *json_str(const char *json, const char *key, char *out, int outlen)
{
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\":\"", key);
    const char *p = strstr(json, pat);
    if (!p) return NULL;
    p += strlen(pat);
    int i = 0;
    while (*p && *p != '"' && i < outlen - 1) out[i++] = *p++;
    out[i] = 0;
    return out;
}

static long json_int(const char *json, const char *key, long def)
{
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    const char *p = strstr(json, pat);
    if (!p) return def;
    p += strlen(pat);
    while (*p == ' ') p++;
    return strtol(p, NULL, 0);
}

/* --- Command handlers --- */

static void cmd_regs(int fd, struct stub_ctx *ctx)
{
    struct armv7m_cpu *c = ctx->cpu;
    char buf[512];
    int n = snprintf(buf, sizeof(buf), "{\"regs\":[");
    for (int i = 0; i < 16; i++)
        n += snprintf(buf + n, sizeof(buf) - n, "%s%u", i ? "," : "", c->r[i]);
    n += snprintf(buf + n, sizeof(buf) - n,
        "],\"xpsr\":%u,\"msp\":%u,\"psp\":%u}",
        c->xpsr, c->msp, c->psp);
    send_resp(fd, buf);
}

static void cmd_mem(int fd, struct stub_ctx *ctx, uint32_t addr, int len)
{
    if (len > 4096) len = 4096;
    char buf[8200];
    int n = snprintf(buf, sizeof(buf), "{\"mem\":\"");
    for (int i = 0; i < len; i++) {
        uint8_t b = membus_read8(ctx->bus, addr + i);
        n += snprintf(buf + n, sizeof(buf) - n, "%02x", b);
    }
    n += snprintf(buf + n, sizeof(buf) - n, "\"}");
    send_resp(fd, buf);
}

static void cmd_writemem(int fd, struct stub_ctx *ctx, uint32_t addr, const char *hex)
{
    int len = strlen(hex) / 2;
    for (int i = 0; i < len; i++) {
        unsigned int b;
        sscanf(hex + i * 2, "%2x", &b);
        membus_write8(ctx->bus, addr + i, b);
    }
    send_resp(fd, "{\"ok\":true}");
}

static void cmd_break(int fd, struct stub_ctx *ctx, uint32_t addr)
{
    if (bp_find(addr) >= 0) { send_resp(fd, "{\"ok\":true}"); return; }
    if (nbp >= MAX_BP) { send_resp(fd, "{\"error\":\"too many breakpoints\"}"); return; }
    bps[nbp].addr = addr;
    bp_patch(*ctx->flash, nbp);
    nbp++;
    send_resp(fd, "{\"ok\":true}");
}

static void cmd_delbreak(int fd, struct stub_ctx *ctx, uint32_t addr)
{
    int idx = bp_find(addr);
    if (idx < 0) { send_resp(fd, "{\"error\":\"breakpoint not found\"}"); return; }
    if (bps[idx].active) bp_unpatch(*ctx->flash, idx);
    bps[idx] = bps[--nbp];
    send_resp(fd, "{\"ok\":true}");
}

static void cmd_listbreak(int fd)
{
    char buf[1024];
    int n = snprintf(buf, sizeof(buf), "{\"breakpoints\":[");
    for (int i = 0; i < nbp; i++)
        n += snprintf(buf + n, sizeof(buf) - n, "%s%u", i ? "," : "", bps[i].addr);
    n += snprintf(buf + n, sizeof(buf) - n, "]}");
    send_resp(fd, buf);
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

/* Continue execution. Check for halt commands via select() every 10K ticks.
 * Returns: 0 = breakpoint hit, 1 = halt requested, 2 = semihost exit */
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
            if (select(fd + 1, &fds, NULL, NULL, &tv) > 0)
                return 1;
        }
    }
}

/* --- Main dispatch --- */

static void dispatch(int fd, struct stub_ctx *ctx, const char *line)
{
    char cmd[32];
    if (!json_str(line, "cmd", cmd, sizeof(cmd))) {
        send_resp(fd, "{\"error\":\"no cmd\"}");
        return;
    }

    if (strcmp(cmd, "regs") == 0) {
        cmd_regs(fd, ctx);
    } else if (strcmp(cmd, "mem") == 0) {
        uint32_t addr = (uint32_t)json_int(line, "addr", 0);
        int len = (int)json_int(line, "len", 64);
        cmd_mem(fd, ctx, addr, len);
    } else if (strcmp(cmd, "writemem") == 0) {
        uint32_t addr = (uint32_t)json_int(line, "addr", 0);
        char hex[8200];
        if (json_str(line, "data", hex, sizeof(hex)))
            cmd_writemem(fd, ctx, addr, hex);
        else
            send_resp(fd, "{\"error\":\"missing data\"}");
    } else if (strcmp(cmd, "step") == 0) {
        single_step(ctx);
        send_stop(fd, ctx);
    } else if (strcmp(cmd, "stepi") == 0) {
        int n = (int)json_int(line, "n", 1);
        for (int i = 0; i < n; i++) single_step(ctx);
        send_stop(fd, ctx);
    } else if (strcmp(cmd, "continue") == 0) {
        int reason = run_continue(fd, ctx);
        if (reason == 2) {
            if (ctx->chardevs) {
                chardev_flush_all(ctx->chardevs);
                chardev_shutdown_all(ctx->chardevs);
            }
            send_resp(fd, "{\"exited\":true}");
            exit(0);
        }
        if (reason == 1) {
            char tmp[256];
            read(fd, tmp, sizeof(tmp));
        }
        send_stop(fd, ctx);
    } else if (strcmp(cmd, "halt") == 0) {
        send_stop(fd, ctx);
    } else if (strcmp(cmd, "reset") == 0) {
        armv7m_cpu_reset(ctx->cpu, ctx->bus);
        send_resp(fd, "{\"ok\":true}");
    } else if (strcmp(cmd, "break") == 0) {
        uint32_t addr = (uint32_t)json_int(line, "addr", 0);
        cmd_break(fd, ctx, addr);
    } else if (strcmp(cmd, "delbreak") == 0) {
        uint32_t addr = (uint32_t)json_int(line, "addr", 0);
        cmd_delbreak(fd, ctx, addr);
    } else if (strcmp(cmd, "listbreak") == 0) {
        cmd_listbreak(fd);
    } else if (strcmp(cmd, "status") == 0) {
        char buf[128];
        snprintf(buf, sizeof(buf),
            "{\"running\":false,\"pc\":%u,\"cycles\":%lu}",
            ctx->cpu->r[REG_PC], (unsigned long)ctx->cpu->cycle_count);
        send_resp(fd, buf);
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "{\"error\":\"unknown cmd: %s\"}", cmd);
        send_resp(fd, buf);
    }
}

/* --- Server --- */

void dbg_stub_run(struct stub_ctx *ctx, int port)
{
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(port),
                                .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
    if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG("Failed to bind debug port %d", port);
        exit(1);
    }
    listen(srv, 1);
    LOG("Debug stub on port %d", port);

    int client = accept(srv, NULL, NULL);
    LOG("Debug client connected");

    send_stop(client, ctx);

    char buf[4096];
    int buf_len = 0;
    while (1) {
        int n = read(client, buf + buf_len, sizeof(buf) - buf_len - 1);
        if (n <= 0) break;
        buf_len += n;
        buf[buf_len] = '\0';

        char *nl;
        while ((nl = strchr(buf, '\n')) != NULL) {
            *nl = '\0';
            LOG("CMD: %s", buf);
            dispatch(client, ctx, buf);
            int remaining = buf_len - (nl - buf + 1);
            memmove(buf, nl + 1, remaining);
            buf_len = remaining;
            buf[buf_len] = '\0';
        }
    }

    bp_unpatch_all(*ctx->flash);
    nbp = 0;
    close(client);
    close(srv);
    LOG("Debug client disconnected");
}
