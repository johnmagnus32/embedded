/*
 * sim-core — ARM Cortex-M4 emulator + debug server
 *
 * Listens on a TCP port. Accepts JSON-line commands, returns JSON-line responses.
 * Device state written to /tmp/sim-state/ files.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "board.h"
#include "dts.h"
#include "elf_sym.h"
#include "state.h"

#define LOG(fmt, ...) fprintf(stderr, "[sim-core] " fmt "\n", ##__VA_ARGS__)
#define DEFAULT_PORT 9001

extern struct board *g_board;

/* Debugger state */
static uint32_t breakpoints[32];
static int nbp = 0;

static int check_breakpoint(struct board *b)
{
    uint32_t pc = b->cpu.r[REG_PC];
    for (int i = 0; i < nbp; i++)
        if (pc == breakpoints[i]) return 1;
    return 0;
}

static void run_until_bp(struct board *b)
{
    do { board_tick(b); } while (!check_breakpoint(b));
}

/* Send a JSON line to the client */
static void send_response(int fd, const char *json)
{
    write(fd, json, strlen(json));
    write(fd, "\n", 1);
}

/* Build and send stop info */
static void send_stop_info(int fd, struct board *b)
{
    uint32_t pc = b->cpu.r[REG_PC];
    uint32_t off;
    const char *fn = sym_lookup(pc, &off);
    int line;
    const char *file = line_lookup(pc, &line);
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"stopped\":true,\"pc\":%u,\"line\":%d,\"func\":\"%s\",\"file\":\"%s\",\"cycles\":%llu}",
        pc, line, fn ? fn : "", file ? file : "", (unsigned long long)b->cpu.cycle_count);
    send_response(fd, buf);
}

static void send_regs(int fd, struct board *b)
{
    char buf[512];
    int n = snprintf(buf, sizeof(buf), "{\"regs\":[");
    for (int i = 0; i < 16; i++)
        n += snprintf(buf + n, sizeof(buf) - n, "%s%u", i ? "," : "", b->cpu.r[i]);
    n += snprintf(buf + n, sizeof(buf) - n, "],\"xpsr\":%u,\"msp\":%u,\"psp\":%u}",
                  b->cpu.xpsr, b->cpu.msp, b->cpu.psp);
    send_response(fd, buf);
}

static void send_mem(int fd, struct board *b, uint32_t addr, int len)
{
    if (len > 1024) len = 1024;
    char buf[4096];
    int n = snprintf(buf, sizeof(buf), "{\"mem\":\"");
    for (int i = 0; i < len; i++) {
        uint8_t byte = 0;
        uint32_t a = addr + i;
        if (a >= RAM_BASE && a < RAM_BASE + RAM_SIZE)
            byte = b->ram[a - RAM_BASE];
        else if (a >= FLASH_BASE && a < FLASH_BASE + FLASH_SIZE)
            byte = b->flash[a - FLASH_BASE];
        n += snprintf(buf + n, sizeof(buf) - n, "%02x", byte);
    }
    n += snprintf(buf + n, sizeof(buf) - n, "\"}");
    send_response(fd, buf);
}

static void send_source(int fd, const char *file)
{
    /* Load source file and send lines */
    char path[512];
    extern char src_search_dir[256];
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

static void handle_command(int fd, struct board *b, const char *line)
{
    /* Minimal JSON parsing — just look for "cmd":"..." */
    const char *cmd = strstr(line, "\"cmd\":\"");
    if (!cmd) return;
    cmd += 7;

    if (strncmp(cmd, "where\"", 6) == 0) {
        send_stop_info(fd, b);

    } else if (strncmp(cmd, "regs\"", 5) == 0) {
        send_regs(fd, b);

    } else if (strncmp(cmd, "mem\"", 4) == 0) {
        uint32_t addr = 0; int len = 64;
        const char *a = strstr(line, "\"addr\":");
        const char *l = strstr(line, "\"len\":");
        if (a) addr = (uint32_t)strtoul(a + 7, NULL, 0);
        if (l) len = atoi(l + 6);
        send_mem(fd, b, addr, len);

    } else if (strncmp(cmd, "step\"", 5) == 0) {
        int orig_line; line_lookup(b->cpu.r[REG_PC], &orig_line);
        do {
            board_tick(b);
            int cur_line; line_lookup(b->cpu.r[REG_PC], &cur_line);
            if (cur_line > 0 && cur_line != orig_line) break;
        } while (1);
        send_stop_info(fd, b);

    } else if (strncmp(cmd, "next\"", 5) == 0) {
        int orig_line; line_lookup(b->cpu.r[REG_PC], &orig_line);
        do {
            uint16_t insn = mem_read16(b->flash, b->ram, b->cpu.r[REG_PC]);
            int is_bl = (insn & 0xF800) == 0xF000;
            if (is_bl) {
                int old_nbp = nbp;
                breakpoints[nbp++] = b->cpu.r[REG_PC] + 4;
                run_until_bp(b);
                nbp = old_nbp;
            } else {
                board_tick(b);
            }
            int cur_line; line_lookup(b->cpu.r[REG_PC], &cur_line);
            if (cur_line > 0 && cur_line != orig_line) break;
        } while (1);
        send_stop_info(fd, b);

    } else if (strncmp(cmd, "continue\"", 9) == 0) {
        do { board_tick(b); } while (!check_breakpoint(b));
        send_stop_info(fd, b);

    } else if (strncmp(cmd, "run\"", 4) == 0) {
        cpu_reset(&b->cpu, b->flash, b->ram);
        do { board_tick(b); } while (!check_breakpoint(b));
        send_stop_info(fd, b);
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

    } else if (strncmp(cmd, "source\"", 7) == 0) {
        const char *f = strstr(line, "\"file\":\"");
        if (f) {
            f += 8;
            char file[256]; int i = 0;
            while (*f && *f != '"' && i < 255) file[i++] = *f++;
            file[i] = '\0';
            send_source(fd, file);
        }

    }
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        LOG("Usage: %s <firmware.elf> <board.dts> [port]", argv[0]);
        return 1;
    }

    int port = (argc >= 4) ? atoi(argv[3]) : DEFAULT_PORT;

    struct dts dt;
    if (dts_parse(&dt, argv[2]) != 0) {
        LOG("Failed to parse DTS: %s", argv[2]);
        return 1;
    }
    LOG("Loaded DTS: %s (%d nodes)", argv[2], dt.nnodes);

    struct board board;
    board_init(&board, &dt);
    board.flash = calloc(1, FLASH_SIZE);
    board.ram   = calloc(1, RAM_SIZE);
    g_board = &board;


    if (elf_load(argv[1], board.flash, board.ram) != 0) {
        LOG("Failed to load ELF: %s", argv[1]);
        return 1;
    }
    LOG("Loaded %s", argv[1]);

    /* Use compilation directory from DWARF for source file lookup */
    extern char elf_comp_dir[512];
    if (elf_comp_dir[0]) {
        char dir[512];
        snprintf(dir, sizeof(dir), "%s/", elf_comp_dir);
        state_set_source_dir(dir);
        LOG("Source dir (from DWARF): %s", elf_comp_dir);
    }

    cpu_reset(&board.cpu, board.flash, board.ram);
    LOG("Ready — waiting for commands");

    /* Start UART TCP server (separate channel for serial output) */
    int uart_port = port + 1;
    if (board.nuarts > 0) {
        if (uart_listen(&board.uarts[0], uart_port) >= 0)
            LOG("UART on port %d", uart_port);
    }

    /* Start debug TCP server */
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(port), .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
    if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG("Failed to bind port %d", port);
        return 1;
    }
    listen(srv, 1);
    LOG("Listening on port %d", port);

    /* Accept one client */
    int client = accept(srv, NULL, NULL);
    LOG("Debug client connected");

    /* Accept UART client (non-blocking — may connect later) */
    if (board.nuarts > 0)
        uart_accept(&board.uarts[0]);
    LOG("UART client connected");

    /* Send initial stop info */
    send_stop_info(client, &board);

    /* Command loop */
    char buf[4096];
    int buf_len = 0;
    while (1) {
        int n = read(client, buf + buf_len, sizeof(buf) - buf_len - 1);
        if (n <= 0) break;
        buf_len += n;
        buf[buf_len] = '\0';

        /* Process complete lines */
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
    free(board.flash);
    free(board.ram);
    return 0;
}
