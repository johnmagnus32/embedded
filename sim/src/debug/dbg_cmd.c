#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "armv7m_cpu.h"
#include "membus.h"
#include "elf_sym.h"
#include "chardev.h"
#include "dbg_server.h"
#include "dbg_cmd.h"
#include "dbg_eval.h"
#include "dbg_tasks.h"

#define LOG(fmt, ...) fprintf(stderr, "[sim-core] " fmt "\n", ##__VA_ARGS__)

static char src_search_dir[256];

void dbg_cmd_set_src_dir(const char *dir)
{
    strncpy(src_search_dir, dir, sizeof(src_search_dir) - 1);
}

/* Debugger state */
static uint32_t breakpoints[32];
static int nbp = 0;

static int check_breakpoint(struct armv7m_cpu *cpu)
{
    uint32_t pc = cpu->r[REG_PC];
    for (int i = 0; i < nbp; i++)
        if (pc == breakpoints[i]) return 1;
    return 0;
}


static void run_until_bp(int fd, struct sim_ctx *ctx)
{
    static uint64_t last_cycles = 0;
    static struct timespec last_ts = {0, 0};
    do {
        int r = ctx->mach->tick(ctx->board);
        if (r & CPU_SEMIHOST_EXIT) {
            int code = r & 0xFF;
            if (ctx->chardevs) {
                /* Accept any pending clients, flush, then graceful shutdown */
                for (int ci = 0; ci < ctx->chardevs->count; ci++)
                    chardev_try_accept(&ctx->chardevs->devs[ci]);
                chardev_flush_all(ctx->chardevs);
                chardev_shutdown_all(ctx->chardevs);
            }
            exit(code);
        }
        if (ctx->cpu->cycle_count - last_cycles >= 1000000) {
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            if (last_ts.tv_sec) {
                double elapsed = (now.tv_sec - last_ts.tv_sec)
                               + (now.tv_nsec - last_ts.tv_nsec) / 1e9;
                double mips = (ctx->cpu->cycle_count - last_cycles) / elapsed / 1e6;
                uint32_t tc = membus_read32(ctx->bus, 0x20000034);
                fprintf(stderr, "\r[perf] %.1f MIPS tc=%u ", mips, tc);
            }
            last_cycles = ctx->cpu->cycle_count;
            last_ts = now;
        }
    } while (!check_breakpoint(ctx->cpu));
    fprintf(stderr, "\n");
}

static void send_stop_info(int fd, struct armv7m_cpu *cpu)
{
    uint32_t pc = cpu->r[REG_PC];
    uint32_t off;
    const char *fn = sym_lookup(pc, &off);
    int line;
    const char *file = line_lookup(pc, &line);
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"stopped\":true,\"pc\":%u,\"line\":%d,\"func\":\"%s\",\"file\":\"%s\",\"cycles\":%lu}",
        pc, line, fn ? fn : "", file ? file : "", (unsigned long)cpu->cycle_count);
    send_response(fd, buf);
}

static void send_regs(int fd, struct armv7m_cpu *cpu)
{
    char buf[512];
    int n = snprintf(buf, sizeof(buf), "{\"regs\":[");
    for (int i = 0; i < 16; i++)
        n += snprintf(buf + n, sizeof(buf) - n, "%s%u", i ? "," : "", cpu->r[i]);
    n += snprintf(buf + n, sizeof(buf) - n, "],\"xpsr\":%u,\"msp\":%u,\"psp\":%u}",
                  cpu->xpsr, cpu->msp, cpu->psp);
    send_response(fd, buf);
}

static void send_mem(int fd, uint8_t *flash, uint8_t *ram, uint32_t addr, int len)
{
    if (len > 1024) len = 1024;
    char buf[4096];
    int n = snprintf(buf, sizeof(buf), "{\"mem\":\"");
    for (int i = 0; i < len; i++) {
        uint8_t byte = 0;
        uint32_t a = addr + i;
        if (a >= RAM_BASE && a < RAM_BASE + RAM_SIZE) byte = ram[a - RAM_BASE];
        else if (a >= FLASH_BASE && a < FLASH_BASE + FLASH_SIZE) byte = flash[a - FLASH_BASE];
        n += snprintf(buf + n, sizeof(buf) - n, "%02x", byte);
    }
    n += snprintf(buf + n, sizeof(buf) - n, "\"}");
    send_response(fd, buf);
}

static void send_source(int fd, const char *file)
{
    char path[512];
    snprintf(path, sizeof(path), "%s%s", src_search_dir, file);
    FILE *f = fopen(path, "r");
    if (!f) { send_response(fd, "{\"lines\":[]}"); return; }

    char buf[65536];
    int n = snprintf(buf, sizeof(buf), "{\"file\":\"%s\",\"lines\":[", file);
    char line[512];
    int first = 1;
    while (fgets(line, sizeof(line), f) && n < (int)sizeof(buf) - 600) {
        line[strcspn(line, "\n\r")] = '\0';
        if (!first) buf[n++] = ',';
        buf[n++] = '"';
        for (char *p = line; *p && n < (int)sizeof(buf) - 10; p++) {
            if (*p == '"' || *p == '\\') buf[n++] = '\\';
            buf[n++] = *p;
        }
        buf[n++] = '"';
        first = 0;
    }
    n += snprintf(buf + n, sizeof(buf) - n, "]}");
    fclose(f);
    send_response(fd, buf);
}

void dbg_dispatch(int fd, struct sim_ctx *ctx, const char *line)
{
    const char *cmd = strstr(line, "\"cmd\":\"");
    if (!cmd) return;
    cmd += 7;

    if (strncmp(cmd, "regs\"", 5) == 0) {
        send_regs(fd, ctx->cpu);

    } else if (strncmp(cmd, "mem\"", 4) == 0) {
        uint32_t addr = 0; int len = 64;
        const char *a = strstr(line, "\"addr\":");
        const char *l = strstr(line, "\"len\":");
        if (a) addr = (uint32_t)strtoul(a + 7, NULL, 0);
        if (l) len = atoi(l + 6);
        send_mem(fd, *ctx->flash, *ctx->ram, addr, len);

    } else if (strncmp(cmd, "step\"", 5) == 0) {
        int orig_line; line_lookup(ctx->cpu->r[REG_PC], &orig_line);
        do {
            ctx->mach->tick(ctx->board);
            int cur_line; line_lookup(ctx->cpu->r[REG_PC], &cur_line);
            if (cur_line > 0 && cur_line != orig_line) break;
        } while (1);
        chardev_flush_all(ctx->chardevs);
        send_stop_info(fd, ctx->cpu);

    } else if (strncmp(cmd, "next\"", 5) == 0) {
        int orig_line; line_lookup(ctx->cpu->r[REG_PC], &orig_line);
        do {
            uint16_t insn = membus_read16(ctx->bus, ctx->cpu->r[REG_PC]);
            uint16_t insn2 = membus_read16(ctx->bus, ctx->cpu->r[REG_PC] + 2);
            /* BL: first halfword 0xF___  with second halfword bit[14:12] = 1x1 */
            int is_bl = (insn & 0xF800) == 0xF000 && (insn2 & 0xD000) == 0xD000;
            if (is_bl) {
                int old_nbp = nbp;
                breakpoints[nbp++] = ctx->cpu->r[REG_PC] + 4;
                run_until_bp(fd, ctx);
                nbp = old_nbp;
            } else {
                ctx->mach->tick(ctx->board);
            }
            int cur_line; line_lookup(ctx->cpu->r[REG_PC], &cur_line);
            if (cur_line > 0 && cur_line != orig_line) break;
        } while (1);
        chardev_flush_all(ctx->chardevs);
        send_stop_info(fd, ctx->cpu);

    } else if (strncmp(cmd, "continue\"", 9) == 0) {
        run_until_bp(fd, ctx);
        chardev_flush_all(ctx->chardevs);
        send_stop_info(fd, ctx->cpu);

    } else if (strncmp(cmd, "run\"", 4) == 0) {
        armv7m_cpu_reset(ctx->cpu, ctx->bus);
        run_until_bp(fd, ctx);
        chardev_flush_all(ctx->chardevs);
        send_stop_info(fd, ctx->cpu);

    } else if (strncmp(cmd, "break\"", 6) == 0) {
        const char *s = strstr(line, "\"spec\":\"");
        if (s) {
            s += 8;
            char spec[64]; int i = 0;
            while (*s && *s != '"' && i < 63) spec[i++] = *s++;
            spec[i] = '\0';
            uint32_t addr = resolve_breakpoint(spec);
            if (addr && nbp < 32) breakpoints[nbp++] = addr;
            char buf[64]; snprintf(buf, sizeof(buf), "{\"addr\":%u}", addr);
            send_response(fd, buf);
        }

    } else if (strncmp(cmd, "delete\"", 7) == 0) {
        const char *n = strstr(line, "\"n\":");
        if (n) {
            int idx = atoi(n + 4);
            if (idx >= 1 && idx <= nbp) {
                for (int i = idx - 1; i < nbp - 1; i++)
                    breakpoints[i] = breakpoints[i + 1];
                nbp--;
            }
        }
        send_response(fd, "{\"ok\":true}");

    } else if (strncmp(cmd, "info\"", 5) == 0) {
        char buf[2048];
        int n = snprintf(buf, sizeof(buf), "{\"breakpoints\":[");
        for (int i = 0; i < nbp; i++) {
            if (i) buf[n++] = ',';
            uint32_t sym_off;
            const char *fn = sym_lookup(breakpoints[i], &sym_off);
            int line_num; const char *file = line_lookup(breakpoints[i], &line_num);
            n += snprintf(buf+n, sizeof(buf)-n, "{\"n\":%d,\"addr\":%u,\"func\":\"%s\",\"file\":\"%s\",\"line\":%d}",
                          i+1, breakpoints[i], fn?fn:"", file?file:"", line_num);
        }
        n += snprintf(buf+n, sizeof(buf)-n, "]}");
        send_response(fd, buf);

    } else if (strncmp(cmd, "source\"", 7) == 0) {
        const char *f = strstr(line, "\"file\":\"");
        if (f) {
            f += 8;
            char file[256]; int i = 0;
            while (*f && *f != '"' && i < 255) file[i++] = *f++;
            file[i] = '\0';
            send_source(fd, file);
        }
    } else if (strncmp(cmd, "memmap\"", 7) == 0) {
        char buf[32768];
        int n = snprintf(buf, sizeof(buf), "{\"msp\":%u,\"psp\":%u,\"ram_base\":%u,\"ram_size\":%u,",
                         ctx->cpu->msp, ctx->cpu->r[REG_SP], (uint32_t)RAM_BASE, (uint32_t)RAM_SIZE);

        const struct elf_section *secs;
        int nsecs = elf_get_sections(&secs);
        n += snprintf(buf+n, sizeof(buf)-n, "\"sections\":[");
        for (int i = 0; i < nsecs; i++) {
            if (i) buf[n++] = ',';
            n += snprintf(buf+n, sizeof(buf)-n, "{\"name\":\"%s\",\"addr\":%u,\"size\":%u}",
                          secs[i].name, secs[i].addr, secs[i].size);
        }
        n += snprintf(buf+n, sizeof(buf)-n, "],\"tasks\":");
        n += dbg_emit_tasks(ctx->cpu, *ctx->flash, *ctx->ram, buf+n, sizeof(buf)-n);

        struct sym_entry globals[64];
        int ng = sym_in_range(RAM_BASE, RAM_BASE + RAM_SIZE, globals, 64);
        n += snprintf(buf+n, sizeof(buf)-n, ",\"globals\":[");
        for (int i = 0; i < ng; i++) {
            if (i) buf[n++] = ',';
            n += snprintf(buf+n, sizeof(buf)-n, "{\"name\":\"%s\",\"addr\":%u,\"size\":%u}",
                          globals[i].name, globals[i].addr, globals[i].size);
        }
        n += snprintf(buf+n, sizeof(buf)-n, "]}");
        send_response(fd, buf);

    } else if (strncmp(cmd, "print\"", 6) == 0) {
        const char *e = strstr(line, "\"expr\":\"");
        if (!e) return;
        e += 8;
        char expr[128]; int ei = 0;
        while (*e && *e != '"' && ei < 127) expr[ei++] = *e++;
        expr[ei] = '\0';

        char rbuf[4096];
        dbg_eval(expr, ctx->cpu, *ctx->flash, *ctx->ram, rbuf, sizeof(rbuf));
        send_response(fd, rbuf);
    }
}
