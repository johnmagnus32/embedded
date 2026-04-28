/*
 * sim-core — ARM Cortex-M4 emulator + debug server
 *
 * Usage: sim-core <elf> --debug <port> [--chardev name=port ...]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "gameboy.h"
#include "membus.h"
#include "chardev.h"
#include "armv7m_nvic.h"
#include "elf_sym.h"

#define LOG(fmt, ...) fprintf(stderr, "[sim-core] " fmt "\n", ##__VA_ARGS__)

/* Source search directory (set from ELF comp_dir) */
static char src_search_dir[256];

static int state_emit_tasks(struct cpu_state *cpu, uint8_t *flash, uint8_t *ram,
                            char *buf, int bufsize)
{
    int n = 0;
    #define P(...) n += snprintf(buf+n, bufsize-n, __VA_ARGS__)

    uint32_t sym_nt = sym_find_by_name("num_tasks");
    uint32_t sym_tasks = sym_find_by_name("tasks");
    uint32_t sym_stacks = sym_find_by_name("task_stacks");
    if (!sym_stacks) sym_stacks = sym_find_by_name("stacks");

    P("[");
    if (sym_nt && sym_tasks && sym_stacks) {
        uint32_t dwarf_tcb_size; int dwarf_sp_off, dwarf_name_off;
        dwarf_get_tcb_layout(&dwarf_tcb_size, &dwarf_sp_off, &dwarf_name_off);

        int tcb_size = dwarf_tcb_size ? (int)dwarf_tcb_size : 32;
        int sp_off   = dwarf_sp_off >= 0 ? dwarf_sp_off : 0;
        int name_off = dwarf_name_off >= 0 ? dwarf_name_off : 4;
        int stk_size = 512;

        uint32_t tasks_off = sym_tasks - RAM_BASE;
        uint32_t stk_base = sym_stacks - RAM_BASE;
        int num_tasks = *(uint32_t *)(ram + (sym_nt - RAM_BASE));
        if (num_tasks > 8) num_tasks = 0;

        for (int t = 0; t < num_tasks; t++) {
            uint32_t sp = *(uint32_t *)(ram + tasks_off + t * tcb_size + sp_off);
            uint32_t name_ptr = *(uint32_t *)(ram + tasks_off + t * tcb_size + name_off);
            char tn[32] = {0};
            if (name_ptr >= FLASH_BASE && name_ptr < FLASH_BASE + FLASH_SIZE) {
                const char *s = (const char *)(flash + (name_ptr - FLASH_BASE));
                for (int j = 0; j < 31 && s[j] >= 0x20 && s[j] < 0x7F; j++) tn[j] = s[j];
            }
            uint32_t stk_top = RAM_BASE + stk_base + (t + 1) * stk_size;
            uint32_t stk_bot = stk_top - stk_size;
            int active = (cpu->r[REG_SP] >= stk_bot && cpu->r[REG_SP] <= stk_top);
            uint32_t dsp = active ? cpu->r[REG_SP] : sp;
            int used = (dsp >= stk_bot && dsp <= stk_top) ? (int)(stk_top - dsp) : 0;

            if (t) P(",");
            P("{\"name\":\"%s\",\"sp\":%u,\"stack_bot\":%u,\"stack_top\":%u,"
              "\"stack_used\":%d,\"stack_size\":%d,\"active\":%s,\"frames\":[",
              tn, dsp, stk_bot, stk_top, used, stk_size, active ? "true" : "false");

            int nframes = 0;
            if (active) {
                uint32_t sym_off2;
                const char *fn = sym_lookup(cpu->r[REG_PC], &sym_off2);
                if (fn) { P("{\"func\":\"%s\",\"sp\":%u}", fn, dsp); nframes++; }
            }
            for (uint32_t sa = dsp; sa < stk_top && nframes < 8; sa += 4) {
                uint32_t val = *(uint32_t *)(ram + (sa - RAM_BASE));
                if (val >= FLASH_BASE + 1 && val < FLASH_BASE + FLASH_SIZE && (val & 1)) {
                    uint32_t sym_off2;
                    const char *fn = sym_lookup(val & ~1u, &sym_off2);
                    if (fn) { if (nframes) P(","); P("{\"func\":\"%s\",\"sp\":%u}", fn, sa); nframes++; }
                }
            }
            P("],\"stack_data\":[");

            struct stack_var svars[32];
            int nsv = 0;
            if (active) nsv = vars_on_stack(cpu->r[REG_PC], svars, 32);

            int nwords = 0;
            for (uint32_t sa = dsp; sa < stk_top && nwords < 128; sa += 4) {
                uint32_t val = *(uint32_t *)(ram + (sa - RAM_BASE));
                if (nwords) P(",");
                P("{\"addr\":%u,\"val\":%u", sa, val);
                if (val >= FLASH_BASE + 1 && val < FLASH_BASE + FLASH_SIZE && (val & 1)) {
                    uint32_t so2;
                    const char *fn2 = sym_lookup(val & ~1u, &so2);
                    if (fn2) P(",\"sym\":\"%s+0x%x\"", fn2, so2);
                }
                for (int vi = 0; vi < nsv; vi++) {
                    uint32_t var_addr = (uint32_t)((int32_t)dsp + svars[vi].sp_offset);
                    if (var_addr == sa) {
                        P(",\"var\":\"%s\"", svars[vi].name);
                        uint32_t bsz = type_byte_size(svars[vi].type_die);
                        if (bsz > 4) P(",\"vsz\":%u", bsz);
                        break;
                    }
                }
                P("}");
                nwords++;
            }
            P("]}");
        }
    }
    P("]");
    #undef P
    return n;
}

/* Debugger state */
static uint32_t breakpoints[32];
static int nbp = 0;

static int check_breakpoint(struct gameboy *b)
{
    uint32_t pc = b->soc.cpu.r[REG_PC];
    for (int i = 0; i < nbp; i++)
        if (pc == breakpoints[i]) return 1;
    return 0;
}

static void run_until_bp(struct gameboy *b)
{
    do { gameboy_tick(b); } while (!check_breakpoint(b));
}

static void send_response(int fd, const char *json)
{
    write(fd, json, strlen(json));
    write(fd, "\n", 1);
}

static void send_stop_info(int fd, struct gameboy *b)
{
    uint32_t pc = b->soc.cpu.r[REG_PC];
    uint32_t off;
    const char *fn = sym_lookup(pc, &off);
    int line;
    const char *file = line_lookup(pc, &line);
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"stopped\":true,\"pc\":%u,\"line\":%d,\"func\":\"%s\",\"file\":\"%s\",\"cycles\":%lu}",
        pc, line, fn ? fn : "", file ? file : "", (unsigned long)b->soc.cpu.cycle_count);
    send_response(fd, buf);
}

static void send_regs(int fd, struct gameboy *b)
{
    char buf[512];
    int n = snprintf(buf, sizeof(buf), "{\"regs\":[");
    for (int i = 0; i < 16; i++)
        n += snprintf(buf + n, sizeof(buf) - n, "%s%u", i ? "," : "", b->soc.cpu.r[i]);
    n += snprintf(buf + n, sizeof(buf) - n, "],\"xpsr\":%u,\"msp\":%u,\"psp\":%u}",
                  b->soc.cpu.xpsr, b->soc.cpu.msp, b->soc.cpu.psp);
    send_response(fd, buf);
}

static void send_mem(int fd, struct gameboy *b, uint32_t addr, int len)
{
    if (len > 1024) len = 1024;
    char buf[4096];
    int n = snprintf(buf, sizeof(buf), "{\"mem\":\"");
    for (int i = 0; i < len; i++) {
        uint8_t byte = 0;
        uint32_t a = addr + i;
        if (a >= RAM_BASE && a < RAM_BASE + RAM_SIZE) byte = b->soc.ram[a - RAM_BASE];
        else if (a >= FLASH_BASE && a < FLASH_BASE + FLASH_SIZE) byte = b->soc.flash[a - FLASH_BASE];
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

static void handle_command(int fd, struct gameboy *b, const char *line)
{
    const char *cmd = strstr(line, "\"cmd\":\"");
    if (!cmd) return;
    cmd += 7;

    if (strncmp(cmd, "regs\"", 5) == 0) {
        send_regs(fd, b);

    } else if (strncmp(cmd, "mem\"", 4) == 0) {
        uint32_t addr = 0; int len = 64;
        const char *a = strstr(line, "\"addr\":");
        const char *l = strstr(line, "\"len\":");
        if (a) addr = (uint32_t)strtoul(a + 7, NULL, 0);
        if (l) len = atoi(l + 6);
        send_mem(fd, b, addr, len);

    } else if (strncmp(cmd, "step\"", 5) == 0) {
        int orig_line; line_lookup(b->soc.cpu.r[REG_PC], &orig_line);
        do {
            gameboy_tick(b);
            int cur_line; line_lookup(b->soc.cpu.r[REG_PC], &cur_line);
            if (cur_line > 0 && cur_line != orig_line) break;
        } while (1);
        send_stop_info(fd, b);

    } else if (strncmp(cmd, "next\"", 5) == 0) {
        int orig_line; line_lookup(b->soc.cpu.r[REG_PC], &orig_line);
        do {
            uint16_t insn = membus_read16(&b->soc.bus, b->soc.cpu.r[REG_PC]);
            int is_bl = (insn & 0xF800) == 0xF000;
            if (is_bl) {
                int old_nbp = nbp;
                breakpoints[nbp++] = b->soc.cpu.r[REG_PC] + 4;
                run_until_bp(b);
                nbp = old_nbp;
            } else {
                gameboy_tick(b);
            }
            int cur_line; line_lookup(b->soc.cpu.r[REG_PC], &cur_line);
            if (cur_line > 0 && cur_line != orig_line) break;
        } while (1);
        send_stop_info(fd, b);

    } else if (strncmp(cmd, "continue\"", 9) == 0) {
        do { gameboy_tick(b); } while (!check_breakpoint(b));
        send_stop_info(fd, b);

    } else if (strncmp(cmd, "run\"", 4) == 0) {
        cpu_reset(&b->soc.cpu, &b->soc.bus);
        do { gameboy_tick(b); } while (!check_breakpoint(b));
        send_stop_info(fd, b);

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
                         b->soc.cpu.msp, b->soc.cpu.r[REG_SP], (uint32_t)RAM_BASE, (uint32_t)RAM_SIZE);

        /* ELF sections */
        const struct elf_section *secs;
        int nsecs = elf_get_sections(&secs);
        n += snprintf(buf+n, sizeof(buf)-n, "\"sections\":[");
        for (int i = 0; i < nsecs; i++) {
            if (i) buf[n++] = ',';
            n += snprintf(buf+n, sizeof(buf)-n, "{\"name\":\"%s\",\"addr\":%u,\"size\":%u}",
                          secs[i].name, secs[i].addr, secs[i].size);
        }
        n += snprintf(buf+n, sizeof(buf)-n, "],\"tasks\":");
        n += state_emit_tasks(&b->soc.cpu, b->soc.flash, b->soc.ram, buf+n, sizeof(buf)-n);

        /* Global symbols in RAM */
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

    } else if (strncmp(cmd, "display\"", 8) == 0) {
        if (b->display && b->display->chardev) {
            /* Dump raw RGB565 framebuffer to display chardev */
            const uint8_t *src = (const uint8_t *)b->display->fb;
            int len = ILI9341_W * ILI9341_H * 2;
            chardev_write_buf(b->display->chardev, src, len);
            char buf[64];
            snprintf(buf, sizeof(buf), "{\"w\":%d,\"h\":%d,\"sz\":%d}", ILI9341_W, ILI9341_H, len);
            send_response(fd, buf);
        } else {
            send_response(fd, "{\"w\":0,\"h\":0}");
        }

    } else if (strncmp(cmd, "gpio\"", 5) == 0) {
        const char *p = strstr(line, "\"pin\":");
        const char *v = strstr(line, "\"val\":");
        if (p && v) {
            int pin = atoi(p + 6);
            int val = atoi(v + 6);
            uint32_t old = b->soc.gpio[0].idr;
            if (val) b->soc.gpio[0].idr |= (1 << pin);
            else     b->soc.gpio[0].idr &= ~(1 << pin);
            /* Trigger EXTI interrupt on rising edge (0→1) for pins 0-4 */
            if (val && !(old & (1 << pin)) && pin <= 4) {
                /* EXTI0=IRQ6(vec22), EXTI1=IRQ7(vec23), ... EXTI4=IRQ10(vec26) */
                armv7m_nvic_set_pending(&b->soc.nvic, 16 + 6 + pin);
            }
        }
        send_response(fd, "{\"ok\":true}");

    } else if (strncmp(cmd, "print\"", 6) == 0) {
        const char *e = strstr(line, "\"expr\":\"");
        if (!e) return;
        e += 8;
        char expr[128]; int ei = 0;
        while (*e && *e != '"' && ei < 127) expr[ei++] = *e++;
        expr[ei] = '\0';

        char rbuf[4096];

        /* Expression evaluator: resolve to (addr, type_die)
         * Supports: varname, *expr, expr.member, expr->member, expr[n] */
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
        int loc = var_lookup(base, b->soc.cpu.r[REG_PC], &reg, &val);
        cur_type = var_type_die(base, b->soc.cpu.r[REG_PC]);
        LOG("expr base='%s' pc=0x%08X loc=%d type=0x%X", base, b->soc.cpu.r[REG_PC], loc, cur_type);

        if (loc == 1) { /* register — value is in the register directly */
            uint32_t regval = b->soc.cpu.r[reg];
            uint32_t scratch = RAM_BASE + RAM_SIZE - 8;
            *(uint32_t*)(b->soc.ram + RAM_SIZE - 8) = regval;
            addr = scratch;
            valid = 1;
        } else if (loc == 2) { /* constant */
            addr = val;
            valid = 1;
        } else if (loc == 3) { /* stack (fbreg — relative to CFA) */
            uint32_t cfa = cfa_offset_at_pc(b->soc.cpu.r[REG_PC]);
            addr = b->soc.cpu.r[REG_SP] + cfa + val;
            valid = 1;
        } else {
            /* Try global symbol */
            uint32_t sym = sym_find_by_name(base);
            if (sym) { addr = sym; valid = 1; }
            /* Try register name */
            static const char *rn[] = {"r0","r1","r2","r3","r4","r5","r6","r7",
                                       "r8","r9","r10","r11","r12","sp","lr","pc"};
            for (int r = 0; r < 16; r++)
                if (strcmp(base, rn[r]) == 0) { addr = b->soc.cpu.r[r]; valid = 1; break; }
        }

        /* Apply leading dereference */
        if (leading_deref && valid && cur_type) {
            uint32_t pointee = type_deref(cur_type);
            if (pointee) {
                uint32_t ptr = addr;
                if (loc != 1 && loc != 2) {
                    if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE)
                        ptr = *(uint32_t*)(b->soc.ram + (addr - RAM_BASE));
                    else if (addr >= FLASH_BASE && addr < FLASH_BASE + FLASH_SIZE)
                        ptr = *(uint32_t*)(b->soc.flash + (addr - FLASH_BASE));
                }
                addr = ptr;
                cur_type = pointee;
            }
        }

        /* Apply chained operators: .member, ->member, [index] */
        while (*p && valid) {
            if (*p == '.' && *(p+1) != '\0') {
                p++;
                char mem[32]; int mi = 0;
                while (*p && *p != '.' && *p != '-' && *p != '[' && mi < 31)
                    mem[mi++] = *p++;
                mem[mi] = '\0';
                uint32_t off, mtype;
                if (type_member(cur_type, mem, &off, &mtype)) {
                    addr += off;
                    cur_type = mtype;
                } else { valid = 0; }

            } else if (*p == '-' && *(p+1) == '>') {
                p += 2;
                uint32_t pointee = type_deref(cur_type);
                if (pointee) {
                    uint32_t ptr = 0;
                    if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE)
                        ptr = *(uint32_t*)(b->soc.ram + (addr - RAM_BASE));
                    else if (addr >= FLASH_BASE && addr < FLASH_BASE + FLASH_SIZE)
                        ptr = *(uint32_t*)(b->soc.flash + (addr - FLASH_BASE));
                    addr = ptr;
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
                } else { valid = 0; }

            } else if (*p == '[') {
                p++;
                char idx_expr[32]; int ii = 0;
                while (*p && *p != ']' && ii < 31) idx_expr[ii++] = *p++;
                idx_expr[ii] = '\0';
                if (*p == ']') p++;

                int idx = atoi(idx_expr);
                if (idx == 0 && idx_expr[0] != '0') {
                    int vreg; uint32_t vval;
                    int vloc = var_lookup(idx_expr, b->soc.cpu.r[REG_PC], &vreg, &vval);
                    if (vloc == 1) idx = (int)b->soc.cpu.r[vreg];
                    else if (vloc == 2) idx = (int)vval;
                    else if (vloc == 3) {
                        uint32_t a = b->soc.cpu.r[REG_SP] + vval;
                        if (a >= RAM_BASE && a < RAM_BASE + RAM_SIZE)
                            idx = (int)*(uint32_t*)(b->soc.ram + (a - RAM_BASE));
                    }
                }

                uint32_t elem_size;
                uint32_t elem_type = type_array_elem(cur_type, &elem_size);
                if (elem_type) {
                    addr += idx * elem_size;
                    cur_type = elem_type;
                } else { valid = 0; }
            } else {
                break;
            }
        }

        if (valid && cur_type) {
            char tbuf[3000];
            type_format(cur_type, addr, b->soc.ram, b->soc.flash, tbuf, sizeof(tbuf));
            snprintf(rbuf, sizeof(rbuf), "{\"expr\":\"%s\",\"val\":\"%s\"}", expr, tbuf);
        } else if (valid) {
            uint32_t v = 0;
            if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE)
                v = *(uint32_t*)(b->soc.ram + (addr - RAM_BASE));
            snprintf(rbuf, sizeof(rbuf), "{\"expr\":\"%s\",\"val\":\"%u\",\"hex\":\"0x%08x\"}", expr, v, v);
        } else {
            snprintf(rbuf, sizeof(rbuf), "{\"expr\":\"%s\",\"error\":\"not found\"}", expr);
        }
        send_response(fd, rbuf);
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        LOG("Usage: %s <elf> --debug <port> [--chardev name=port ...]", argv[0]);
        return 1;
    }

    const char *elf_path = argv[1];
    int debug_port = 9001;

    /* Parse --debug and --chardev args */
    struct chardev_table chardevs;
    chardev_table_init(&chardevs);

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0 && i + 1 < argc) {
            debug_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--chardev") == 0 && i + 1 < argc) {
            chardev_add(&chardevs, argv[++i]);
        }
    }

    /* Start chardev listeners */
    chardev_listen_all(&chardevs);

    /* Create board */
    struct gameboy board;
    gameboy_init(&board, &chardevs);

    /* Load firmware */
    if (elf_load(elf_path, board.soc.flash, board.soc.ram) != 0) {
        LOG("Failed to load ELF: %s", elf_path);
        return 1;
    }
    LOG("Loaded %s", elf_path);

    extern char elf_comp_dir[512];
    if (elf_comp_dir[0]) {
        char dir[512];
        snprintf(dir, sizeof(dir), "%s/", elf_comp_dir);
        strncpy(src_search_dir, dir, sizeof(src_search_dir) - 1);
    }

    cpu_reset(&board.soc.cpu, &board.soc.bus);

    /* Start debug server */
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(debug_port), .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
    if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG("Failed to bind debug port %d", debug_port);
        return 1;
    }
    listen(srv, 1);
    LOG("Debug server on port %d", debug_port);

    int client = accept(srv, NULL, NULL);
    LOG("Debug client connected");

    send_stop_info(client, &board);

    /* Command loop */
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
            handle_command(client, &board, buf);
            int remaining = buf_len - (nl - buf + 1);
            memmove(buf, nl + 1, remaining);
            buf_len = remaining;
            buf[buf_len] = '\0';
        }
    }

    close(client);
    close(srv);
    free(board.soc.flash);
    free(board.soc.ram);
    return 0;
}
