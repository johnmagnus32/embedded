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

/* --- RSP helpers --- */

static uint32_t parse_le32(const char *hex)
{
    uint32_t v = 0;
    for (int b = 0; b < 4; b++) {
        unsigned int byte;
        sscanf(hex + b * 2, "%2x", &byte);
        v |= byte << (b * 8);
    }
    return v;
}

/* Read memory from stub */
static int read_mem(struct dbg_client *c, uint32_t addr, uint8_t *out, int len)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "m%x,%x", addr, len);
    dbg_send(c, cmd);
    char *resp = dbg_recv(c);
    if (!resp) return -1;
    for (int i = 0; i < len && resp[i*2] && resp[i*2+1]; i++) {
        unsigned int b;
        sscanf(resp + i * 2, "%2x", &b);
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

/* Get registers: parse 'g' response (r0-r15 as LE hex, skip FPA, then xPSR) */
static int get_regs(struct dbg_client *c, uint32_t regs[16])
{
    dbg_send(c, "g");
    char *resp = dbg_recv(c);
    if (!resp || strlen(resp) < 16 * 8) return -1;
    for (int i = 0; i < 16; i++)
        regs[i] = parse_le32(resp + i * 8);
    return 0;
}

static uint32_t get_pc(struct dbg_client *c)
{
    uint32_t regs[16];
    if (get_regs(c, regs) < 0) return 0;
    return regs[15];
}

/* --- Stop info display --- */

void dbg_show_stop(const char *resp)
{
    (void)resp;  /* RSP stop reply is just "S05" — no PC in it */
    /* We need to read PC separately — caller should call show_current_location */
}

static void show_location(struct dbg_client *c)
{
    uint32_t pc = get_pc(c);
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
    dbg_send(c, "c");
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
            dbg_recv(c);  /* consume S05 */
            show_location(c);
            break;
        }
        if (got_sigint) {
            got_sigint = 0;
            printf("[halting...]\n");
            dbg_send_halt(c);
            dbg_recv(c);
            show_location(c);
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
        uint32_t pc = get_pc(client);
        int orig_line;
        const char *orig_file = line_lookup(pc, &orig_line);

        for (int i = 0; i < 1000; i++) {
            dbg_send(client, "s");
            dbg_recv(client);  /* S05 */
            uint32_t new_pc = get_pc(client);
            int new_line;
            const char *new_file = line_lookup(new_pc, &new_line);
            if (!orig_file || !new_file ||
                new_line != orig_line || strcmp(new_file, orig_file) != 0) {
                show_location(client);
                break;
            }
        }
    }
    else if (strcmp(line, "next") == 0 || strcmp(line, "n") == 0) {
        uint32_t pc = get_pc(client);
        uint8_t insn_bytes[4];
        read_mem(client, pc, insn_bytes, 4);
        uint16_t insn16 = insn_bytes[0] | (insn_bytes[1] << 8);
        uint16_t insn16b = insn_bytes[2] | (insn_bytes[3] << 8);

        int is_bl = ((insn16 >> 11) == 0x1E && (insn16b >> 12) == 0xD);
        int is_blx = (insn16 & 0xFF80) == 0x4780;

        if (is_bl || is_blx) {
            uint32_t ret_addr = is_bl ? pc + 4 : pc + 2;
            char cmd[64];
            snprintf(cmd, sizeof(cmd), "Z0,%x,2", ret_addr);
            dbg_send(client, cmd);
            dbg_recv(client);
            do_continue(client);
            snprintf(cmd, sizeof(cmd), "z0,%x,2", ret_addr);
            dbg_send(client, cmd);
            dbg_recv(client);
        } else {
            dbg_send(client, "s");
            dbg_recv(client);
            show_location(client);
        }
    }
    else if (strcmp(line, "halt") == 0 || strcmp(line, "h") == 0) {
        /* Already stopped if we're at the prompt */
        show_location(client);
    }
    else if (strncmp(line, "break ", 6) == 0 || strncmp(line, "b ", 2) == 0) {
        const char *spec = line + (line[1] == ' ' ? 2 : 6);
        uint32_t addr = resolve_breakpoint(spec);
        if (!addr) { printf("Cannot resolve: %s\n", spec); return 0; }
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "Z0,%x,2", addr);
        dbg_send(client, cmd);
        char *resp = dbg_recv(client);
        if (resp && strcmp(resp, "OK") == 0) {
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
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "z0,%x,2", addr);
        dbg_send(client, cmd);
        dbg_recv(client);
        printf("Deleted breakpoint at 0x%08x\n", addr);
    }
    else if (strcmp(line, "info regs") == 0 || strcmp(line, "info r") == 0 ||
             strcmp(line, "regs") == 0) {
        uint32_t regs[16];
        if (get_regs(client, regs) < 0) return 0;
        static const char *names[] = {
            "r0","r1","r2","r3","r4","r5","r6","r7",
            "r8","r9","r10","r11","r12","sp","lr","pc"
        };
        for (int i = 0; i < 16; i++) {
            printf("%-4s= 0x%08x", names[i], regs[i]);
            printf((i % 4 == 3) ? "\n" : "  ");
        }
        /* xPSR is after FPA registers in the 'g' response */
        dbg_send(client, "g");
        char *resp = dbg_recv(client);
        if (resp && strlen(resp) >= 16 * 8 + 192 + 8) {
            uint32_t xpsr = parse_le32(resp + 16 * 8 + 192);
            printf("xpsr= 0x%08x\n", xpsr);
        }
    }
    else if (strcmp(line, "info tasks") == 0) {
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
            int len = strlen(buf);
            if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0';
            printf("%s %3d  %s\n", ln == cur_line ? "→" : " ", ln, buf);
        }
        fclose(f);
    }
    else if (strcmp(line, "backtrace") == 0 || strcmp(line, "bt") == 0) {
        uint32_t regs[16];
        if (get_regs(client, regs) < 0) return 0;
        uint32_t pc = regs[15];

        uint32_t off;
        const char *fn = sym_lookup(pc, &off);
        int ln;
        const char *file = line_lookup(pc, &ln);
        printf("#0  %s+0x%x", fn ? fn : "???", off);
        if (file) printf(" at %s:%d", file, ln);
        printf("\n");

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
        printf("Reset not supported over GDB RSP.\n");
    }
    else if (strcmp(line, "status") == 0) {
        uint32_t pc = get_pc(client);
        printf("Stopped at 0x%08x\n", pc);
    }
    else if (strncmp(line, "print ", 6) == 0 || strncmp(line, "p ", 2) == 0) {
        const char *expr = line + (line[1] == ' ' ? 2 : 6);
        uint32_t regs[16];
        if (get_regs(client, regs) < 0) { printf("Cannot read registers.\n"); return 0; }
        uint32_t pc = regs[15];

        const char *p = expr;
        uint32_t addr = 0;
        uint32_t cur_type = 0;
        int valid = 0;

        /* Leading * dereference */
        int leading_deref = 0;
        if (*p == '*') { leading_deref = 1; p++; }

        /* Parse base variable name */
        char base[64]; int bi = 0;
        while (*p && *p != '.' && *p != '-' && *p != '[' && bi < 63)
            base[bi++] = *p++;
        base[bi] = '\0';

        /* Resolve base variable */
        int reg; uint32_t val;
        int loc = var_lookup(base, pc, &reg, &val);
        cur_type = var_type_die(base, pc);

        if (loc == 1) { /* register */
            addr = regs[reg];
            valid = 1;
            /* For register vars, addr IS the value — need special handling */
            if (cur_type && !*p && !leading_deref) {
                /* Simple register variable — just print the value */
                printf("%s = %u (0x%08x)\n", expr, addr, addr);
                return 0;
            }
        } else if (loc == 2) { /* constant */
            addr = val;
            valid = 1;
        } else if (loc == 3) { /* stack (fbreg) */
            uint32_t cfa = cfa_offset_at_pc(pc);
            addr = regs[13] + cfa + val;  /* r13 = SP */
            valid = 1;
        } else {
            /* Try global symbol */
            uint32_t sym = sym_find_by_name(base);
            if (sym) { addr = sym; valid = 1; }
            /* Try register name */
            static const char *rn[] = {"r0","r1","r2","r3","r4","r5","r6","r7",
                                       "r8","r9","r10","r11","r12","sp","lr","pc"};
            for (int r = 0; r < 16; r++)
                if (strcmp(base, rn[r]) == 0) {
                    printf("%s = %u (0x%08x)\n", base, regs[r], regs[r]);
                    return 0;
                }
        }

        /* Apply leading dereference */
        if (leading_deref && valid && cur_type) {
            uint32_t pointee = type_deref(cur_type);
            if (pointee) {
                addr = read_mem32(client, addr);
                cur_type = pointee;
            }
        }

        /* Apply chained operators: .member, ->member, [index] */
        while (*p && valid) {
            if (*p == '.' && *(p+1)) {
                p++;
                char mem[32]; int mi = 0;
                while (*p && *p != '.' && *p != '-' && *p != '[' && mi < 31)
                    mem[mi++] = *p++;
                mem[mi] = '\0';
                uint32_t off, mtype;
                if (type_member(cur_type, mem, &off, &mtype)) {
                    addr += off;
                    cur_type = mtype;
                } else { valid = 0; printf("No member '%s'\n", mem); }
            } else if (*p == '-' && *(p+1) == '>') {
                p += 2;
                uint32_t pointee = type_deref(cur_type);
                if (pointee) {
                    addr = read_mem32(client, addr);
                    cur_type = pointee;
                }
                char mem[32]; int mi = 0;
                while (*p && *p != '.' && *p != '-' && *p != '[' && mi < 31)
                    mem[mi++] = *p++;
                mem[mi] = '\0';
                uint32_t off, mtype;
                if (type_member(cur_type, mem, &off, &mtype)) {
                    addr += off;
                    cur_type = mtype;
                } else { valid = 0; printf("No member '%s'\n", mem); }
            } else if (*p == '[') {
                p++;
                char idx_str[32]; int ii = 0;
                while (*p && *p != ']' && ii < 31) idx_str[ii++] = *p++;
                idx_str[ii] = '\0';
                if (*p == ']') p++;
                int idx = atoi(idx_str);
                uint32_t elem_size;
                uint32_t elem_type = type_array_elem(cur_type, &elem_size);
                if (elem_type) {
                    addr += idx * elem_size;
                    cur_type = elem_type;
                } else { valid = 0; printf("Not an array.\n"); }
            } else break;
        }

        if (valid) {
            uint32_t bsz = type_byte_size(cur_type);
            if (bsz <= 4) {
                uint32_t v = read_mem32(client, addr);
                if (bsz == 1) v &= 0xFF;
                else if (bsz == 2) v &= 0xFFFF;
                printf("%s = %u (0x%x)\n", expr, v, v);
            } else {
                /* Larger type — read and show as hex dump */
                if (bsz > 64) bsz = 64;
                uint8_t data[64];
                read_mem(client, addr, data, bsz);
                printf("%s @ 0x%08x = {", expr, addr);
                for (uint32_t i = 0; i < bsz; i++)
                    printf("%s%02x", i ? " " : "", data[i]);
                printf("}\n");
            }
        } else if (!valid) {
            printf("Cannot resolve: %s\n", expr);
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
               "  print <expr> (p) — evaluate expression (var, var.member, var->field, var[i])\n"
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
