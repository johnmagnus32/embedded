/*
 * vis.c — 2x2 grid visualizer for the ARM Cortex-M4 emulator
 *
 * ┌──────────────────┬──────────────────┐
 * │  Memory Map      │  UART Console    │
 * │  (regs, flash,   │                  │
 * │   sram, tasks)   │                  │
 * ├──────────────────┼──────────────────┤
 * │  Source Code     │  Debugger Log    │
 * │  (→ arrow)       │  (commands/out)  │
 * └──────────────────┴──────────────────┘
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "cpu.h"
#include "vis.h"
#include "elf_sym.h"

/* ── UART console ring buffer ── */
#define CON_LINES 32
#define CON_WIDTH 60
static char con[CON_LINES][CON_WIDTH + 1];
static int con_row = 0, con_col = 0, con_count = 0;
static int ctx_switches = 0;

void vis_console_putc(char c)
{
    if (c == '\0') return;
    if (c == '\r') { con_col = 0; return; }
    if (c == '\n' || con_col >= CON_WIDTH) {
        con_row = (con_row + 1) % CON_LINES;
        con_col = 0; con_count++;
        memset(con[con_row], 0, CON_WIDTH + 1);
        if (c == '\n') return;
    }
    if (c >= 32) con[con_row][con_col++] = c;
}

/* ── Debugger log ring buffer ── */
#define DBG_LINES 32
#define DBG_WIDTH 60
static char dbg[DBG_LINES][DBG_WIDTH + 1];
static int dbg_row = 0, dbg_count = 0;

void vis_dbg_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    dbg_row = (dbg_row + 1) % DBG_LINES;
    dbg_count++;
    vsnprintf(dbg[dbg_row], DBG_WIDTH, fmt, ap);
    va_end(ap);
}

/* ── ANSI helpers ── */
#define ESC    "\033["
#define CLEAR  ESC "2J" ESC "H"
#define BOLD   ESC "1m"
#define DIM    ESC "2m"
#define GREEN  ESC "32m"
#define YELLOW ESC "33m"
#define CYAN   ESC "36m"
#define RED    ESC "31m"
#define RESET  ESC "0m"

static FILE *g_out;
static int g_cols, g_rows, g_half_c, g_half_r;

static void at(int r, int c) { fprintf(g_out, ESC "%d;%dH", r, c); }

static void cell(int row, int col, int w, const char *s)
{
    at(row, col);
    int vis = 0;
    for (const char *p = s; *p; p++) {
        if (*p == '\033') { while (*p && *p != 'm') p++; }
        else vis++;
    }
    fprintf(g_out, "%s", s);
    for (int i = vis; i < w; i++) fputc(' ', g_out);
}

static char *fmt(const char *f, ...)
{
    static char buf[256];
    va_list ap;
    va_start(ap, f);
    vsnprintf(buf, sizeof(buf), f, ap);
    va_end(ap);
    return buf;
}

static void get_size(void)
{
    struct winsize w;
    if (ioctl(STDERR_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 40) {
        g_cols = w.ws_col; g_rows = w.ws_row;
    } else {
        g_cols = 80; g_rows = 40;
    }
    g_half_c = g_cols / 2;
    g_half_r = g_rows / 2;
}

/* ── Source file cache ── */
static char *src_lines[4096];
static int src_nlines;
static char src_file[256];

static void load_source(const char *file)
{
    if (!file || strcmp(file, src_file) == 0) return;
    for (int i = 0; i < src_nlines; i++) free(src_lines[i]);
    src_nlines = 0;
    strncpy(src_file, file, sizeof(src_file) - 1);
    FILE *f = fopen(file, "r");
    if (!f) return;
    char buf[512];
    while (fgets(buf, sizeof(buf), f) && src_nlines < 4096) {
        int len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') buf[--len] = '\0';
        if (len > 0 && buf[len-1] == '\r') buf[--len] = '\0';
        src_lines[src_nlines++] = strdup(buf);
    }
    fclose(f);
}

static const char *con_line(int i)
{
    return con[(con_row - i + CON_LINES) % CON_LINES];
}

static const char *dbg_line(int i)
{
    return dbg[(dbg_row - i + DBG_LINES) % DBG_LINES];
}

void vis_dump(FILE *out, struct cpu_state *cpu, uint8_t *flash, uint8_t *ram,
              const char *event)
{
    g_out = out;
    get_size();
    int lw = g_half_c - 2;          /* left pane width */
    int rw = g_cols - g_half_c - 3; /* right pane width */
    int rc = g_half_c + 3;          /* right pane col start */

    uint32_t pc  = cpu->r[REG_PC];
    uint32_t psp = cpu->psp;
    uint32_t msp = cpu->msp;
    int in_flash = (pc >= FLASH_BASE && pc < FLASH_BASE + FLASH_SIZE);

    if (strstr(event, "PendSV")) ctx_switches++;

    fprintf(out, CLEAR);

    /* ── Draw grid lines ── */
    /* Vertical divider full height */
    for (int r = 1; r <= g_rows - 1; r++) {
        at(r, g_half_c + 1); fprintf(out, DIM "│" RESET);
    }
    /* Horizontal divider */
    for (int c = 1; c <= g_cols; c++) {
        at(g_half_r, c);
        if (c == g_half_c + 1) fprintf(out, DIM "┼" RESET);
        else fprintf(out, DIM "─" RESET);
    }

    /* ════════════════════════════════════════════
     * TOP-LEFT: Memory Map + Registers
     * ════════════════════════════════════════════ */
    int row = 1;
    cell(row, 2, lw, fmt(BOLD "%-24s" RESET DIM " cy %-7llu ctx %d" RESET,
         event, (unsigned long long)cpu->cycle_count, ctx_switches));
    row++;

    /* Registers */
    uint32_t sym_off;
    const char *fn = sym_lookup(pc, &sym_off);
    cell(row, 2, lw, fmt("PC " CYAN "%08X" RESET " %s " GREEN "%s" RESET,
         pc, cpu->in_handler ? YELLOW "[H]" RESET : "[T]",
         fn ? fn : ""));
    row++;
    cell(row, 2, lw, fmt("PSP " CYAN "%08X" RESET "  MSP " CYAN "%08X" RESET, psp, msp));
    row++;

    /* Flash */
    cell(row, 2, lw, "08000000 ┌──────────────────┐"); row++;
    if (in_flash)
        cell(row, 2, lw, fmt(CYAN "PC →" RESET "     │ " CYAN "%-14s" RESET " │", fn ? fn : ".text"));
    else
        cell(row, 2, lw, "         │ .text            │");
    row++;
    cell(row, 2, lw, "08080000 └──────────────────┘"); row++;

    /* SRAM */
    cell(row, 2, lw, "20000000 ┌──────────────────┐"); row++;
    cell(row, 2, lw, "         │ .data + .bss     │"); row++;

    /* Tasks */
    int num_tasks = *(uint32_t *)(ram);
    if (num_tasks > 0 && num_tasks <= 8) {
        for (int t = 0; t < num_tasks && row < g_half_r - 3; t++) {
            uint32_t sp = *(uint32_t *)(ram + 0x308 + t * 8);
            uint32_t name_ptr = *(uint32_t *)(ram + 0x308 + t * 8 + 4);
            char tn[12] = {0};
            if (name_ptr >= FLASH_BASE && name_ptr < FLASH_BASE + FLASH_SIZE) {
                const char *s = (const char *)(flash + (name_ptr - FLASH_BASE));
                for (int j = 0; j < 7 && s[j] >= 0x20 && s[j] < 0x7F; j++) tn[j] = s[j];
            }
            if (!tn[0]) { tn[0] = '0' + t; tn[1] = '\0'; }
            int active = (sp >= RAM_BASE && sp <= RAM_BASE + 0x2000 && psp >= sp && psp <= sp + 256);
            if (active)
                cell(row, 2, lw, fmt(CYAN "PSP→" RESET "     │" GREEN "▶%-6s" RESET CYAN "%08X" RESET "│", tn, psp));
            else
                cell(row, 2, lw, fmt("         │ %-6s" DIM "%08X" RESET "│", tn, sp));
            row++;
        }
    }
    cell(row, 2, lw, fmt(YELLOW "MSP→" RESET "     │ stack  " DIM "%08X" RESET "│", msp)); row++;
    cell(row, 2, lw, "20020000 └──────────────────┘"); row++;

    /* ════════════════════════════════════════════
     * TOP-RIGHT: UART Console
     * ════════════════════════════════════════════ */
    cell(1, rc, rw, BOLD "UART Console" RESET);
    int con_avail = g_half_r - 3;
    int con_total = (con_count < CON_LINES) ? con_count + 1 : CON_LINES;
    int con_show = (con_total < con_avail) ? con_total : con_avail;
    for (int i = 0; i < con_show; i++)
        cell(2 + i, rc, rw, fmt(DIM "%s" RESET, con_line(con_show - 1 - i)));

    /* ════════════════════════════════════════════
     * BOTTOM-LEFT: Source Code
     * ════════════════════════════════════════════ */
    int src_top = g_half_r + 1;
    int src_avail = g_rows - g_half_r - 2;
    int cur_line;
    const char *file = line_lookup(pc, &cur_line);
    if (file) load_source(file);

    if (src_nlines > 0 && cur_line > 0) {
        cell(src_top, 2, lw, fmt(DIM "%s" RESET, file));
        int context = src_avail / 2;
        int start = cur_line - context;
        if (start < 1) start = 1;
        int end = start + src_avail - 2;
        if (end > src_nlines) end = src_nlines;

        for (int l = start; l <= end; l++) {
            int r = src_top + 1 + (l - start);
            if (r >= g_rows) break;
            const char *lt = (l <= src_nlines) ? src_lines[l - 1] : "";
            char tr[128];
            int maxw = lw - 7;
            if (maxw > 120) maxw = 120;
            strncpy(tr, lt, maxw); tr[maxw] = '\0';
            for (char *p = tr; *p; p++) if (*p == '\t') *p = ' ';
            if (l == cur_line)
                cell(r, 2, lw, fmt(CYAN "→%3d" RESET " " BOLD "%s" RESET, l, tr));
            else
                cell(r, 2, lw, fmt(DIM " %3d" RESET " %s", l, tr));
        }
    } else {
        cell(src_top, 2, lw, DIM "  (no source — compile with -g)" RESET);
    }

    /* ════════════════════════════════════════════
     * BOTTOM-RIGHT: Debugger Log
     * ════════════════════════════════════════════ */
    cell(src_top, rc, rw, BOLD "Debugger" RESET);
    int dbg_avail = g_rows - g_half_r - 2;
    int dbg_total = (dbg_count < DBG_LINES) ? dbg_count + 1 : DBG_LINES;
    int dbg_show = (dbg_total < dbg_avail) ? dbg_total : dbg_avail;
    for (int i = 0; i < dbg_show; i++)
        cell(src_top + 1 + i, rc, rw, fmt(DIM "%s" RESET, dbg_line(dbg_show - 1 - i)));

    /* Position cursor at bottom for prompt */
    at(g_rows, 1);
    fflush(out);
}
