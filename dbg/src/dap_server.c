/*
 * dap_server.c — Debug Adapter Protocol server
 *
 * Reads DAP requests from stdin, translates to GDB RSP commands,
 * writes DAP responses/events to stdout.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include "dap.h"
#include "client.h"
#include "elf_sym.h"

/* --- Minimal JSON helpers --- */

static const char *json_get_str(const char *json, const char *key, char *out, int outlen)
{
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\":\"", key);
    const char *p = strstr(json, pat);
    if (!p) { out[0] = 0; return NULL; }
    p += strlen(pat);
    int i = 0;
    while (*p && *p != '"' && i < outlen - 1) out[i++] = *p++;
    out[i] = 0;
    return out;
}

static int json_get_int(const char *json, const char *key, int def)
{
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    const char *p = strstr(json, pat);
    if (!p) return def;
    p += strlen(pat);
    while (*p == ' ') p++;
    return atoi(p);
}

/* --- DAP response/event formatting --- */

static int dap_seq = 1;

static void dap_response(int req_seq, const char *command, const char *body)
{
    char buf[8192];
    snprintf(buf, sizeof(buf),
        "{\"seq\":%d,\"type\":\"response\",\"request_seq\":%d,"
        "\"success\":true,\"command\":\"%s\",\"body\":{%s}}",
        dap_seq++, req_seq, command, body ? body : "");
    dap_write(buf);
}

static void dap_event(const char *event, const char *body)
{
    char buf[4096];
    snprintf(buf, sizeof(buf),
        "{\"seq\":%d,\"type\":\"event\",\"event\":\"%s\",\"body\":{%s}}",
        dap_seq++, event, body ? body : "");
    dap_write(buf);
}

/* --- RSP helpers (reused from cmd.c pattern) --- */

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

static int rsp_get_regs(struct dbg_client *c, uint32_t regs[16])
{
    dbg_send(c, "g");
    char *resp = dbg_recv(c);
    if (!resp || strlen(resp) < 16 * 8) return -1;
    for (int i = 0; i < 16; i++)
        regs[i] = parse_le32(resp + i * 8);
    return 0;
}

static int rsp_read_mem(struct dbg_client *c, uint32_t addr, uint8_t *out, int len)
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

static uint32_t rsp_read_mem32(struct dbg_client *c, uint32_t addr)
{
    uint8_t buf[4];
    if (rsp_read_mem(c, addr, buf, 4) < 0) return 0;
    return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

/* --- Stopped event helper --- */

static void send_stopped_event(struct dbg_client *c, const char *reason)
{
    uint32_t regs[16];
    rsp_get_regs(c, regs);
    uint32_t pc = regs[15];
    int line;
    const char *file = line_lookup(pc, &line);
    char body[256];
    snprintf(body, sizeof(body),
        "\"reason\":\"%s\",\"threadId\":1,\"allThreadsStopped\":true", reason);
    dap_event("stopped", body);
    (void)file; (void)line;
}

/* --- Async continue loop --- */

static void do_dap_continue(struct dbg_client *c)
{
    dbg_send(c, "c");

    /* select() on both RSP socket and stdin */
    while (1) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(c->fd, &fds);
        FD_SET(STDIN_FILENO, &fds);
        int maxfd = c->fd > STDIN_FILENO ? c->fd : STDIN_FILENO;
        struct timeval tv = {0, 100000}; /* 100ms */
        int ready = select(maxfd + 1, &fds, NULL, NULL, &tv);
        if (ready <= 0) continue;

        if (FD_ISSET(c->fd, &fds)) {
            /* Stop reply from stub */
            dbg_recv(c);  /* consume S05 or W00 */
            send_stopped_event(c, "breakpoint");
            return;
        }
        if (FD_ISSET(STDIN_FILENO, &fds)) {
            /* DAP request while running */
            char *msg = dap_read();
            if (!msg) return;
            char command[64];
            json_get_str(msg, "command", command, sizeof(command));
            int seq = json_get_int(msg, "seq", 0);
            if (strcmp(command, "pause") == 0) {
                dbg_send_halt(c);
                dbg_recv(c);  /* consume S05 */
                dap_response(seq, "pause", "");
                send_stopped_event(c, "pause");
                free(msg);
                return;
            } else if (strcmp(command, "disconnect") == 0) {
                dap_response(seq, "disconnect", "");
                free(msg);
                exit(0);
            }
            free(msg);
        }
    }
}

/* --- Source-level step (same logic as REPL) --- */

static void do_dap_step(struct dbg_client *c)
{
    uint32_t regs[16];
    rsp_get_regs(c, regs);
    uint32_t pc = regs[15];
    int orig_line;
    const char *orig_file = line_lookup(pc, &orig_line);

    for (int i = 0; i < 1000; i++) {
        dbg_send(c, "s");
        dbg_recv(c);
        rsp_get_regs(c, regs);
        uint32_t new_pc = regs[15];
        int new_line;
        const char *new_file = line_lookup(new_pc, &new_line);
        if (!orig_file || !new_file ||
            new_line != orig_line || strcmp(new_file, orig_file) != 0)
            break;
    }
}

/* --- Next (step over) --- */

static void do_dap_next(struct dbg_client *c)
{
    uint32_t regs[16];
    rsp_get_regs(c, regs);
    uint32_t pc = regs[15];
    uint8_t insn_bytes[4];
    rsp_read_mem(c, pc, insn_bytes, 4);
    uint16_t insn16 = insn_bytes[0] | (insn_bytes[1] << 8);
    uint16_t insn16b = insn_bytes[2] | (insn_bytes[3] << 8);

    int is_bl = ((insn16 >> 11) == 0x1E && (insn16b >> 12) == 0xD);
    int is_blx = (insn16 & 0xFF80) == 0x4780;

    if (is_bl || is_blx) {
        uint32_t ret_addr = is_bl ? pc + 4 : pc + 2;
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "Z0,%x,2", ret_addr);
        dbg_send(c, cmd);
        dbg_recv(c);
        dbg_send(c, "c");
        dbg_recv(c);  /* wait for stop */
        snprintf(cmd, sizeof(cmd), "z0,%x,2", ret_addr);
        dbg_send(c, cmd);
        dbg_recv(c);
    } else {
        do_dap_step(c);
    }
}

/* --- Breakpoint tracking --- */

#define MAX_DAP_BP 64
static struct { char file[256]; int line; uint32_t addr; } dap_bps[MAX_DAP_BP];
static int ndap_bps;

/* --- Main DAP dispatch --- */

void dap_server_run(const char *connect_str, const char *elf_path)
{
    struct dbg_client client = {0};
    client.fd = -1;

    /* Redirect stderr to a log file so it doesn't corrupt DAP stdout */
    freopen("/tmp/sim-dbg-dap.log", "w", stderr);
    fprintf(stderr, "DAP server started\n"); fflush(stderr);

    while (1) {
        char *msg = dap_read();
        if (!msg) break;

        char command[64];
        json_get_str(msg, "command", command, sizeof(command));
        int seq = json_get_int(msg, "seq", 0);
        fprintf(stderr, "DAP request: seq=%d cmd=%s\n", seq, command); fflush(stderr);

        if (strcmp(command, "initialize") == 0) {
            dap_response(seq, "initialize",
                "\"supportsConfigurationDoneRequest\":true,"
                "\"supportsFunctionBreakpoints\":true,"
                "\"supportsEvaluateForHovers\":true");
            dap_event("initialized", "");
        }
        else if (strcmp(command, "attach") == 0) {
            char target[128], elf[512];
            json_get_str(msg, "gdbTarget", target, sizeof(target));
            json_get_str(msg, "executable", elf, sizeof(elf));

            /* Parse host:port */
            char host[128] = "127.0.0.1";
            int port = 1234;
            char *colon = strrchr(target, ':');
            if (colon) {
                int hlen = (int)(colon - target);
                if (hlen > 0 && hlen < (int)sizeof(host)) {
                    memcpy(host, target, hlen);
                    host[hlen] = '\0';
                }
                port = atoi(colon + 1);
            }

            /* Load ELF */
            static uint8_t dummy_flash[1024*1024];
            static uint8_t dummy_ram[128*1024];
            elf_load(elf[0] ? elf : elf_path, dummy_flash, dummy_ram);

            /* Connect to stub */
            if (dbg_connect(&client, host, port) < 0) {
                fprintf(stderr, "Cannot connect to %s:%d\n", host, port);
                dap_response(seq, "attach", "");
                free(msg);
                continue;
            }
            /* Initial handshake */
            dbg_send(&client, "?");
            dbg_recv(&client);

            dap_response(seq, "attach", "");
        }
        else if (strcmp(command, "configurationDone") == 0) {
            dap_response(seq, "configurationDone", "");
            /* Auto-continue to first breakpoint instead of stopping at entry */
            do_dap_continue(&client);
        }
        else if (strcmp(command, "threads") == 0) {
            dap_response(seq, "threads",
                "\"threads\":[{\"id\":1,\"name\":\"main\"}]");
        }
        else if (strcmp(command, "setBreakpoints") == 0) {
            char src_path[512];
            json_get_str(msg, "path", src_path, sizeof(src_path));
            fprintf(stderr, "setBreakpoints: file=%s\n", src_path); fflush(stderr);

            /* Remove existing breakpoints for this file */
            for (int i = 0; i < ndap_bps; i++) {
                if (strcmp(dap_bps[i].file, src_path) == 0 && dap_bps[i].addr) {
                    char cmd[64];
                    snprintf(cmd, sizeof(cmd), "z0,%x,2", dap_bps[i].addr);
                    dbg_send(&client, cmd);
                    dbg_recv(&client);
                    dap_bps[i] = dap_bps[--ndap_bps];
                    i--;
                }
            }

            /* Parse and set new breakpoints */
            char bp_body[4096];
            int bn = 0;
            bn += snprintf(bp_body + bn, sizeof(bp_body) - bn, "\"breakpoints\":[");

            /* Find breakpoint lines in the JSON */
            const char *p = strstr(msg, "\"breakpoints\":[");
            if (p) {
                p = strchr(p, '[') + 1;
                while (*p) {
                    const char *lp = strstr(p, "\"line\":");
                    if (!lp) break;
                    int line = atoi(lp + 7);

                    /* Extract filename from path for resolve_breakpoint */
                    const char *fname = strrchr(src_path, '/');
                    fname = fname ? fname + 1 : src_path;
                    char spec[256];
                    snprintf(spec, sizeof(spec), "%s:%d", fname, line);
                    uint32_t addr = resolve_breakpoint(spec);
                    fprintf(stderr, "  bp: %s -> 0x%08x\n", spec, addr); fflush(stderr);

                    if (addr && ndap_bps < MAX_DAP_BP) {
                        char cmd[64];
                        snprintf(cmd, sizeof(cmd), "Z0,%x,2", addr);
                        dbg_send(&client, cmd);
                        dbg_recv(&client);
                        strncpy(dap_bps[ndap_bps].file, src_path, 255);
                        dap_bps[ndap_bps].line = line;
                        dap_bps[ndap_bps].addr = addr;
                        ndap_bps++;
                    }

                    int verified = addr ? 1 : 0;
                    if (bn > 16) bn += snprintf(bp_body + bn, sizeof(bp_body) - bn, ",");
                    bn += snprintf(bp_body + bn, sizeof(bp_body) - bn,
                        "{\"verified\":%s,\"line\":%d}",
                        verified ? "true" : "false", line);

                    /* Advance past this breakpoint object */
                    p = strchr(lp, '}');
                    if (!p) break;
                    p++;
                }
            }
            bn += snprintf(bp_body + bn, sizeof(bp_body) - bn, "]");
            fprintf(stderr, "  response: %s\n", bp_body); fflush(stderr);
            dap_response(seq, "setBreakpoints", bp_body);
        }
        else if (strcmp(command, "setFunctionBreakpoints") == 0) {
            dap_response(seq, "setFunctionBreakpoints", "\"breakpoints\":[]");
        }
        else if (strcmp(command, "continue") == 0) {
            dap_response(seq, "continue", "\"allThreadsContinued\":true");
            do_dap_continue(&client);
        }
        else if (strcmp(command, "next") == 0) {
            do_dap_next(&client);
            dap_response(seq, "next", "");
            send_stopped_event(&client, "step");
        }
        else if (strcmp(command, "stepIn") == 0) {
            do_dap_step(&client);
            dap_response(seq, "stepIn", "");
            send_stopped_event(&client, "step");
        }
        else if (strcmp(command, "stepOut") == 0) {
            /* Step out: continue until LR is reached */
            uint32_t regs[16];
            rsp_get_regs(&client, regs);
            uint32_t lr = regs[14] & ~1u;
            if (lr > 0x08000000 && lr < 0x08100000) {
                char cmd[64];
                snprintf(cmd, sizeof(cmd), "Z0,%x,2", lr);
                dbg_send(&client, cmd);
                dbg_recv(&client);
                dbg_send(&client, "c");
                dbg_recv(&client);
                snprintf(cmd, sizeof(cmd), "z0,%x,2", lr);
                dbg_send(&client, cmd);
                dbg_recv(&client);
            }
            dap_response(seq, "stepOut", "");
            send_stopped_event(&client, "step");
        }
        else if (strcmp(command, "pause") == 0) {
            dbg_send_halt(&client);
            dbg_recv(&client);
            dap_response(seq, "pause", "");
            send_stopped_event(&client, "pause");
        }
        else if (strcmp(command, "stackTrace") == 0) {
            uint32_t regs[16];
            rsp_get_regs(&client, regs);
            uint32_t pc = regs[15];
            uint32_t lr = regs[14] & ~1u;

            char body[2048];
            int n = 0;
            int nframes = 0;
            n += snprintf(body + n, sizeof(body) - n, "\"stackFrames\":[");

            /* Frame 0 */
            uint32_t off;
            const char *fn = sym_lookup(pc, &off);
            int line = 0;
            const char *file = line_lookup(pc, &line);
            if (line < 1) line = 1;
            extern char elf_comp_dir[512];
            {
                char full[1024];
                if (file)
                    snprintf(full, sizeof(full), "%s/%s", elf_comp_dir, file);
                if (nframes > 0) n += snprintf(body + n, sizeof(body) - n, ",");
                n += snprintf(body + n, sizeof(body) - n,
                    "{\"id\":%d,\"name\":\"%s\",\"line\":%d,\"column\":1",
                    nframes, fn ? fn : "???", line);
                if (file)
                    n += snprintf(body + n, sizeof(body) - n,
                        ",\"source\":{\"name\":\"%s\",\"path\":\"%s\"}",
                        file, full);
                n += snprintf(body + n, sizeof(body) - n, "}");
                nframes++;
            }

            /* Frame 1 from LR */
            if (lr > 0x08000000 && lr < 0x08100000) {
                fn = sym_lookup(lr, &off);
                file = line_lookup(lr, &line);
                if (line < 1) line = 1;
                char full[1024];
                if (file)
                    snprintf(full, sizeof(full), "%s/%s", elf_comp_dir, file);
                n += snprintf(body + n, sizeof(body) - n,
                    ",{\"id\":%d,\"name\":\"%s\",\"line\":%d,\"column\":1",
                    nframes, fn ? fn : "???", line);
                if (file)
                    n += snprintf(body + n, sizeof(body) - n,
                        ",\"source\":{\"name\":\"%s\",\"path\":\"%s\"}",
                        file, full);
                n += snprintf(body + n, sizeof(body) - n, "}");
                nframes++;
            }

            n += snprintf(body + n, sizeof(body) - n, "],\"totalFrames\":%d", nframes);
            dap_response(seq, "stackTrace", body);
        }
        else if (strcmp(command, "scopes") == 0) {
            dap_response(seq, "scopes",
                "\"scopes\":[{\"name\":\"Locals\",\"variablesReference\":1,"
                "\"expensive\":false},"
                "{\"name\":\"Registers\",\"variablesReference\":2,"
                "\"expensive\":false}]");
        }
        else if (strcmp(command, "variables") == 0) {
            int ref = json_get_int(msg, "variablesReference", 0);
            char body[8192];
            int n = 0;
            n += snprintf(body + n, sizeof(body) - n, "\"variables\":[");

            if (ref == 2) {
                /* Registers */
                uint32_t regs[16];
                rsp_get_regs(&client, regs);
                static const char *names[] = {
                    "r0","r1","r2","r3","r4","r5","r6","r7",
                    "r8","r9","r10","r11","r12","sp","lr","pc"
                };
                for (int i = 0; i < 16; i++) {
                    if (i > 0) n += snprintf(body + n, sizeof(body) - n, ",");
                    n += snprintf(body + n, sizeof(body) - n,
                        "{\"name\":\"%s\",\"value\":\"0x%08x\",\"variablesReference\":0}",
                        names[i], regs[i]);
                }
            } else {
                /* Locals — use DWARF */
                uint32_t regs[16];
                rsp_get_regs(&client, regs);
                uint32_t pc = regs[15];
                struct stack_var vars[32];
                int nv = vars_on_stack(pc, vars, 32);
                uint32_t cfa = cfa_offset_at_pc(pc);
                for (int i = 0; i < nv; i++) {
                    uint32_t addr = regs[13] + cfa + vars[i].sp_offset;
                    uint32_t val = rsp_read_mem32(&client, addr);
                    if (i > 0) n += snprintf(body + n, sizeof(body) - n, ",");
                    n += snprintf(body + n, sizeof(body) - n,
                        "{\"name\":\"%s\",\"value\":\"%u (0x%x)\",\"variablesReference\":0}",
                        vars[i].name, val, val);
                }
            }

            n += snprintf(body + n, sizeof(body) - n, "]");
            dap_response(seq, "variables", body);
        }
        else if (strcmp(command, "evaluate") == 0) {
            char expr[256];
            json_get_str(msg, "expression", expr, sizeof(expr));
            /* Strip leading "p " if user typed it */
            const char *e = expr;
            if (e[0] == 'p' && e[1] == ' ') e += 2;
            fprintf(stderr, "  evaluate: '%s'\n", e); fflush(stderr);

            /* Try global symbol */
            uint32_t addr = sym_find_by_name(e);
            fprintf(stderr, "  sym_find: 0x%08x\n", addr); fflush(stderr);
            if (addr) {
                uint32_t val = rsp_read_mem32(&client, addr);
                char body[256];
                snprintf(body, sizeof(body),
                    "\"result\":\"%u (0x%x)\",\"variablesReference\":0", val, val);
                dap_response(seq, "evaluate", body);
            } else {
                /* Try as local variable via DWARF */
                uint32_t regs[16];
                rsp_get_regs(&client, regs);
                uint32_t pc = regs[15];
                int reg; uint32_t val;
                int loc = var_lookup(e, pc, &reg, &val);
                fprintf(stderr, "  var_lookup: loc=%d\n", loc); fflush(stderr);
                if (loc == 1) {
                    char body[256];
                    snprintf(body, sizeof(body),
                        "\"result\":\"%u (0x%x)\",\"variablesReference\":0",
                        regs[reg], regs[reg]);
                    dap_response(seq, "evaluate", body);
                } else if (loc == 3) {
                    uint32_t cfa = cfa_offset_at_pc(pc);
                    uint32_t a = regs[13] + cfa + val;
                    uint32_t v = rsp_read_mem32(&client, a);
                    char body[256];
                    snprintf(body, sizeof(body),
                        "\"result\":\"%u (0x%x)\",\"variablesReference\":0", v, v);
                    dap_response(seq, "evaluate", body);
                } else {
                    dap_response(seq, "evaluate",
                        "\"result\":\"<unknown>\",\"variablesReference\":0");
                }
            }
        }
        else if (strcmp(command, "disconnect") == 0) {
            dap_event("terminated", "");
            dap_response(seq, "disconnect", "");
            dbg_close(&client);
            free(msg);
            break;
        }
        else {
            /* Unknown command — send empty success response */
            dap_response(seq, command, "");
        }

        free(msg);
    }
}
