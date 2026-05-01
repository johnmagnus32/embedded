/*
 * dbg_cmd.c — sim-dbg command handlers
 *
 * Translates GDB-like commands into stub protocol JSON calls.
 * Uses elf_sym for symbol/DWARF resolution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/select.h>
#include "dbg_client.h"
#include "dbg_cmd.h"
#include "elf_sym.h"

/* --- JSON helpers --- */

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

static int json_bool(const char *json, const char *key)
{
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\":true", key);
    return strstr(json, pat) != NULL;
}

static const char *json_str_start(const char *json, const char *key)
{
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\":\"", key);
    const char *p = strstr(json, pat);
    if (!p) return NULL;
    return p + strlen(pat);
}

/* Read memory from stub into local buffer */
static int read_mem(struct dbg_client *c, uint32_t addr, uint8_t *out, int len)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "{\"cmd\":\"mem\",\"addr\":%u,\"len\":%d}", addr, len);
    dbg_send(c, cmd);
    char *resp = dbg_recv(c);
    if (!resp) return -1;
    const char *hex = json_str_start(resp, "mem");
    if (!hex) return -1;
    for (int i = 0; i < len && hex[i*2] && hex[i*2+1]; i++) {
        unsigned int b;
        sscanf(hex + i * 2, "%2x", &b);
        out[i] = b;
    }
    return 0;
}

static uint32_t read_mem32(struct dbg_client *c, uint32_t addr)
{
    uint8_t buf[4];
    if (read_mem(c, addr, buf, 4) < 0) return 0;
    return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

/* Get registers from stub */
static uint32_t get_pc(struct dbg_client *c)
{
    dbg_send(c, "{\"cmd\":\"regs\"}");
    char *resp = dbg_recv(c);
    if (!resp) return 0;
    /* PC is regs[15] */
    const char *p = strstr(resp, "\"regs\":[");
    if (!p) return 0;
    p += 8;
    uint32_t val = 0;
    for (int i = 0; i <= 15; i++) {
        val = strtoul(p, NULL, 0);
        if (i < 15) { p = strchr(p, ','); if (!p) return 0; p++; }
    }
    return val;
}

/* --- Stop info display --- */

void dbg_show_stop(const char *json)
{
    uint32_t pc = (uint32_t)json_int(json, "pc", 0);
    uint32_t off;
    const char *fn = sym_lookup(pc, &off);
    int line;
    const char *file = line_lookup(pc, &line);

    if (fn && file)
        printf("%s+0x%x at %s:%d (0x%08x)\n", fn, off, file, line, pc);
    else if (fn)
        printf("%s+0x%x (0x%08x)\n", fn, off, pc);
    else
        printf("0x%08x\n", pc);
}

/* --- Async continue with Ctrl+C support --- */

static volatile sig_atomic_t got_sigint;

static void sigint_handler(int sig) { (void)sig; got_sigint = 1; }

static void do_continue(struct dbg_client *c)
{
    dbg_send(c, "{\"cmd\":\"continue\"}");
    printf("[running...]\n");

    got_sigint = 0;
    struct sigaction sa = {.sa_handler = sigint_handler};
    struct sigaction old_sa;
    sigaction(SIGINT, &sa, &old_sa);

    while (1) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(c->fd, &fds);
        struct timeval tv = {0, 100000}; /* 100ms */
        int ready = select(c->fd + 1, &fds, NULL, NULL, &tv);

        if (ready > 0 && FD_ISSET(c->fd, &fds)) {
            char *resp = dbg_recv(c);
            if (resp) dbg_show_stop(resp);
            break;
        }
        if (got_sigint) {
            got_sigint = 0;
            printf("[halting...]\n");
            dbg_send(c, "{\"cmd\":\"halt\"}");
            char *resp = dbg_recv(c);
            if (resp) dbg_show_stop(resp);
            break;
        }
    }

    sigaction(SIGINT, &old_sa, NULL);
}

/* --- Command dispatch --- */

int dbg_handle_command(struct dbg_client *client, const char *line)
{
    while (*line == ' ') line++;
    if (!*line) return 0;

    if (strcmp(line, "quit") == 0 || strcmp(line, "q") == 0)
        return -1;

    if (strcmp(line, "continue") == 0 || strcmp(line, "c") == 0) {
        do_continue(client);
    }
    else if (strcmp(line, "step") == 0 || strcmp(line, "s") == 0) {
        /* Source-level step: step instructions until DWARF line changes */
        uint32_t pc = get_pc(client);
        int orig_line;
        const char *orig_file = line_lookup(pc, &orig_line);

        for (int i = 0; i < 1000; i++) {
            dbg_send(client, "{\"cmd\":\"step\"}");
            char *resp = dbg_recv(client);
            if (!resp) break;
            uint32_t new_pc = (uint32_t)json_int(resp, "pc", 0);
            int new_line;
            const char *new_file = line_lookup(new_pc, &new_line);
            if (!orig_file || !new_file ||
                new_line != orig_line || strcmp(new_file, orig_file) != 0) {
                dbg_show_stop(resp);
                break;
            }
        }
    }
    else if (strcmp(line, "next") == 0 || strcmp(line, "n") == 0) {
        /* Step over: if at a BL/BLX, set temp breakpoint at return addr */
        uint32_t pc = get_pc(client);
        uint8_t insn_bytes[4];
        read_mem(client, pc, insn_bytes, 4);
        uint16_t insn16 = insn_bytes[0] | (insn_bytes[1] << 8);
        uint16_t insn16b = insn_bytes[2] | (insn_bytes[3] << 8);

        int is_bl = ((insn16 >> 11) == 0x1E && (insn16b >> 12) == 0xD);  /* BL */
        int is_blx = (insn16 & 0xFF80) == 0x4780;  /* BLX reg */

        if (is_bl || is_blx) {
            uint32_t ret_addr = is_bl ? pc + 4 : pc + 2;
            char cmd[128];
            snprintf(cmd, sizeof(cmd), "{\"cmd\":\"break\",\"addr\":%u}", ret_addr);
            dbg_send(client, cmd);
            dbg_recv(client); /* consume ok */
            do_continue(client);
            snprintf(cmd, sizeof(cmd), "{\"cmd\":\"delbreak\",\"addr\":%u}", ret_addr);
            dbg_send(client, cmd);
            dbg_recv(client);
        } else {
            /* Not a call — same as step */
            dbg_send(client, "{\"cmd\":\"step\"}");
            char *resp = dbg_recv(client);
            if (resp) dbg_show_stop(resp);
        }
    }
    else if (strcmp(line, "halt") == 0 || strcmp(line, "h") == 0) {
        dbg_send(client, "{\"cmd\":\"halt\"}");
        char *resp = dbg_recv(client);
        if (resp) dbg_show_stop(resp);
    }
    else if (strncmp(line, "break ", 6) == 0 || strncmp(line, "b ", 2) == 0) {
        const char *spec = line + (line[1] == ' ' ? 2 : 6);
        uint32_t addr = resolve_breakpoint(spec);
        if (!addr) {
            printf("Cannot resolve: %s\n", spec);
            return 0;
        }
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "{\"cmd\":\"break\",\"addr\":%u}", addr);
        dbg_send(client, cmd);
        char *resp = dbg_recv(client);
        if (resp && json_bool(resp, "ok")) {
            uint32_t off;
            const char *fn = sym_lookup(addr, &off);
            int ln;
            const char *file = line_lookup(addr, &ln);
            if (fn && file)
                printf("Breakpoint at 0x%08x: %s, %s:%d\n", addr, fn, file, ln);
            else
                printf("Breakpoint at 0x%08x\n", addr);
        }
    }
    else if (strncmp(line, "delete ", 7) == 0 || strncmp(line, "d ", 2) == 0) {
        const char *spec = line + (line[1] == ' ' ? 2 : 7);
        uint32_t addr = strtoul(spec, NULL, 0);
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "{\"cmd\":\"delbreak\",\"addr\":%u}", addr);
        dbg_send(client, cmd);
        dbg_recv(client);
        printf("Deleted breakpoint at 0x%08x\n", addr);
    }
    else if (strcmp(line, "info break") == 0 || strcmp(line, "info b") == 0) {
        dbg_send(client, "{\"cmd\":\"listbreak\"}");
        char *resp = dbg_recv(client);
        if (!resp) return 0;
        /* Parse breakpoints array */
        const char *p = strstr(resp, "[");
        if (!p) { printf("No breakpoints.\n"); return 0; }
        p++;
        int n = 0;
        while (*p && *p != ']') {
            uint32_t addr = strtoul(p, NULL, 0);
            if (addr) {
                uint32_t off;
                const char *fn = sym_lookup(addr, &off);
                int ln;
                const char *file = line_lookup(addr, &ln);
                if (fn && file)
                    printf("  #%d  0x%08x  %s at %s:%d\n", ++n, addr, fn, file, ln);
                else
                    printf("  #%d  0x%08x\n", ++n, addr);
            }
            p = strchr(p, ',');
            if (!p) break;
            p++;
        }
        if (n == 0) printf("No breakpoints.\n");
    }
    else if (strcmp(line, "info regs") == 0 || strcmp(line, "info r") == 0 ||
             strcmp(line, "regs") == 0) {
        dbg_send(client, "{\"cmd\":\"regs\"}");
        char *resp = dbg_recv(client);
        if (!resp) return 0;
        const char *p = strstr(resp, "\"regs\":[");
        if (!p) return 0;
        p += 8;
        static const char *names[] = {
            "r0","r1","r2","r3","r4","r5","r6","r7",
            "r8","r9","r10","r11","r12","sp","lr","pc"
        };
        for (int i = 0; i < 16; i++) {
            uint32_t val = strtoul(p, NULL, 0);
            printf("%-4s= 0x%08x", names[i], val);
            printf((i % 4 == 3) ? "\n" : "  ");
            p = strchr(p, ',');
            if (!p) break;
            p++;
        }
        uint32_t xpsr = (uint32_t)json_int(resp, "xpsr", 0);
        uint32_t msp = (uint32_t)json_int(resp, "msp", 0);
        uint32_t psp = (uint32_t)json_int(resp, "psp", 0);
        printf("xpsr= 0x%08x  msp = 0x%08x  psp = 0x%08x\n", xpsr, msp, psp);
    }
    else if (strcmp(line, "info tasks") == 0) {
        /* Read task info via memory reads using DWARF TCB layout */
        uint32_t tcb_size;
        int sp_off, name_off;
        dwarf_get_tcb_layout(&tcb_size, &sp_off, &name_off);
        if (!tcb_size) { printf("No TCB layout in DWARF.\n"); return 0; }

        uint32_t num_addr = sym_find_by_name("num_tasks");
        uint32_t tasks_addr = sym_find_by_name("tasks");
        if (!num_addr || !tasks_addr) { printf("Cannot find task symbols.\n"); return 0; }

        uint32_t num = read_mem32(client, num_addr);
        printf("  %-4s %-12s %-12s %s\n", "ID", "Name", "SP", "State");
        for (uint32_t i = 0; i < num && i < 32; i++) {
            uint32_t base = tasks_addr + i * tcb_size;
            uint32_t sp = read_mem32(client, base + sp_off);
            char name[16] = {0};
            if (name_off >= 0) {
                uint32_t name_ptr = read_mem32(client, base + name_off);
                read_mem(client, name_ptr, (uint8_t *)name, 15);
            }
            printf("  %-4d %-12s 0x%08x\n", i, name, sp);
        }
    }
    else if (strcmp(line, "list") == 0 || strcmp(line, "l") == 0) {
        uint32_t pc = get_pc(client);
        int cur_line;
        const char *file = line_lookup(pc, &cur_line);
        if (!file) { printf("No source info at 0x%08x\n", pc); return 0; }

        /* Read source file */
        extern char elf_comp_dir[512];
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", elf_comp_dir, file);
        FILE *f = fopen(path, "r");
        if (!f) f = fopen(file, "r");
        if (!f) { printf("Cannot open %s\n", file); return 0; }

        char buf[256];
        int ln = 0;
        int start = cur_line > 5 ? cur_line - 5 : 1;
        int end = cur_line + 5;
        while (fgets(buf, sizeof(buf), f)) {
            ln++;
            if (ln < start) continue;
            if (ln > end) break;
            /* Remove trailing newline */
            int len = strlen(buf);
            if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0';
            printf("%s %3d  %s\n", ln == cur_line ? "→" : " ", ln, buf);
        }
        fclose(f);
    }
    else if (strcmp(line, "backtrace") == 0 || strcmp(line, "bt") == 0) {
        uint32_t pc = get_pc(client);
        /* Simple backtrace: show current frame, then follow LR */
        dbg_send(client, "{\"cmd\":\"regs\"}");
        char *resp = dbg_recv(client);
        if (!resp) return 0;

        /* Parse LR (regs[14]) */
        const char *p = strstr(resp, "\"regs\":[");
        if (!p) return 0;
        p += 8;
        uint32_t regs[16];
        for (int i = 0; i < 16; i++) {
            regs[i] = strtoul(p, NULL, 0);
            p = strchr(p, ',');
            if (!p) break;
            p++;
        }

        /* Frame 0: current PC */
        uint32_t off;
        const char *fn = sym_lookup(pc, &off);
        int ln;
        const char *file = line_lookup(pc, &ln);
        printf("#0  %s+0x%x", fn ? fn : "???", off);
        if (file) printf(" at %s:%d", file, ln);
        printf("\n");

        /* Frame 1: LR */
        uint32_t lr = regs[14] & ~1u;
        if (lr > 0x08000000 && lr < 0x08100000) {
            fn = sym_lookup(lr, &off);
            file = line_lookup(lr, &ln);
            printf("#1  %s+0x%x", fn ? fn : "???", off);
            if (file) printf(" at %s:%d", file, ln);
            printf("\n");
        }
    }
    else if (strcmp(line, "reset") == 0) {
        dbg_send(client, "{\"cmd\":\"reset\"}");
        dbg_recv(client);
        printf("CPU reset.\n");
    }
    else if (strcmp(line, "status") == 0) {
        dbg_send(client, "{\"cmd\":\"status\"}");
        char *resp = dbg_recv(client);
        if (resp) {
            int running = json_bool(resp, "running");
            uint32_t pc = (uint32_t)json_int(resp, "pc", 0);
            uint64_t cycles = (uint64_t)json_int(resp, "cycles", 0);
            printf("%s at 0x%08x, %lu cycles\n",
                   running ? "Running" : "Stopped", pc, (unsigned long)cycles);
        }
    }
    else if (strcmp(line, "help") == 0) {
        printf("Commands:\n"
               "  continue (c)     — run until breakpoint or Ctrl+C\n"
               "  step (s)         — source-level single step\n"
               "  next (n)         — step over function calls\n"
               "  halt (h)         — stop execution\n"
               "  break <spec> (b) — set breakpoint (function, file:line, 0xaddr)\n"
               "  delete <addr> (d)— delete breakpoint\n"
               "  info break       — list breakpoints\n"
               "  info regs        — show registers\n"
               "  info tasks       — show RTOS tasks\n"
               "  list (l)         — show source around current line\n"
               "  backtrace (bt)   — show call stack\n"
               "  reset            — reset CPU\n"
               "  status           — show run state\n"
               "  quit (q)         — disconnect and exit\n");
    }
    else {
        printf("Unknown command: %s (type 'help' for commands)\n", line);
    }

    return 0;
}
