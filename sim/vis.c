/*
 * vis.c — Two-pane visualizer for the ARM Cortex-M4 emulator
 *
 * Left pane: CPU state + source code (or memory map if no debug info).
 * Right pane: UART console output.
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
static int con_row = 0;
static int con_col = 0;
static int con_count = 0;
static int ctx_switches = 0;

void vis_console_putc(char c)
{
    if (c == '\0') return;
    if (c == '\r') { con_col = 0; return; }
    if (c == '\n' || con_col >= CON_WIDTH) {
        con_row = (con_row + 1) % CON_LINES;
        con_col = 0;
        con_count++;
        memset(con[con_row], 0, CON_WIDTH + 1);
        if (c == '\n') return;
    }
    if (c >= 32) con[con_row][con_col++] = c;
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
static int g_cols, g_half;

static void at(int row, int col) { fprintf(g_out, ESC "%d;%dH", row, col); }

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

static const char *con_line(int i)
{
    return con[(con_row - i + CON_LINES) % CON_LINES];
}

static int get_cols(void)
{
    struct winsize w;
    if (ioctl(STDERR_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 40)
        return w.ws_col;
    return 80;
}

/* ── Source file cache ── */
static char *src_lines[4096];
static int src_nlines;
static char src_file[256];

static void load_source(const char *file)
{
    if (!file || strcmp(file, src_file) == 0) return;
    /* Free old */
    for (int i = 0; i < src_nlines; i++) free(src_lines[i]);
    src_nlines = 0;
    strncpy(src_file, file, sizeof(src_file) - 1);

    FILE *f = fopen(file, "r");
    if (!f) return;
    char buf[512];
    while (fgets(buf, sizeof(buf), f) && src_nlines < 4096) {
        /* Strip trailing newline */
        int len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') buf[--len] = '\0';
        if (len > 0 && buf[len-1] == '\r') buf[--len] = '\0';
        src_lines[src_nlines++] = strdup(buf);
    }
    fclose(f);
}

void vis_dump(FILE *out, struct cpu_state *cpu, uint8_t *flash, uint8_t *ram,
              const char *event)
{
    g_out = out;
    g_cols = get_cols();
    g_half = g_cols / 2;
    int lw = g_half - 2;
    int rw = g_cols - g_half - 3;
    int rcol = g_half + 3;

    uint32_t pc  = cpu->r[REG_PC];
    uint32_t psp = cpu->psp;
    uint32_t msp = cpu->msp;

    if (strstr(event, "PendSV")) ctx_switches++;

    fprintf(out, CLEAR);

    /* Divider */
    for (int r = 1; r <= 30; r++) { at(r, g_half + 1); fprintf(out, DIM "│" RESET); }

    int row = 1;

    /* ── Header ── */
    cell(row, 2, lw, fmt(BOLD "%-28s" RESET DIM "  cy %-8llu ctx %d" RESET,
         event, (unsigned long long)cpu->cycle_count, ctx_switches));
    cell(row, rcol, rw, BOLD "UART Console" RESET);
    row++;

    /* ── Registers ── */
    row++;
    uint32_t sym_off;
    const char *fn = sym_lookup(pc, &sym_off);
    cell(row, 2, lw, fmt("  PC  " CYAN "0x%08X" RESET "  %s",
         pc, cpu->in_handler ? YELLOW "[HANDLER]" RESET : GREEN "[THREAD]" RESET));
    row++;
    if (fn)
        cell(row, 2, lw, fmt("      " GREEN "%s" RESET "+" CYAN "0x%X" RESET, fn, sym_off));
    row++;
    cell(row, 2, lw, fmt("  PSP " CYAN "0x%08X" RESET "  MSP " CYAN "0x%08X" RESET, psp, msp));
    row++;

    /* ── Source code or memory map ── */
    int cur_line;
    const char *file = line_lookup(pc, &cur_line);
    if (file) load_source(file);

    row++;
    if (src_nlines > 0 && cur_line > 0) {
        /* Show source code centered on current line */
        int context = 8;  /* lines above/below */
        int start = cur_line - context;
        if (start < 1) start = 1;
        int end = cur_line + context;
        if (end > src_nlines) end = src_nlines;

        cell(row, 2, lw, fmt(DIM "  %s" RESET, file));
        row++;

        for (int l = start; l <= end && row <= 28; l++) {
            const char *line_text = (l <= src_nlines) ? src_lines[l - 1] : "";
            /* Truncate to fit */
            char trunc[128];
            int maxw = lw - 8;
            if (maxw > 120) maxw = 120;
            strncpy(trunc, line_text, maxw);
            trunc[maxw] = '\0';
            /* Replace tabs with spaces */
            for (char *p = trunc; *p; p++) if (*p == '\t') *p = ' ';

            if (l == cur_line)
                cell(row, 2, lw, fmt(CYAN " →%3d" RESET " " BOLD "%s" RESET, l, trunc));
            else
                cell(row, 2, lw, fmt(DIM "  %3d" RESET " %s", l, trunc));
            row++;
        }
    } else {
        /* Fallback: memory map (no debug info) */
        int in_flash = (pc >= FLASH_BASE && pc < FLASH_BASE + FLASH_SIZE);
        cell(row, 2, lw, BOLD "  FLASH" RESET DIM " 512K" RESET); row++;
        cell(row, 2, lw, "  0x08000000 ┌────────────────────┐"); row++;
        if (in_flash)
            cell(row, 2, lw, fmt("  " CYAN "PC →" RESET "     │ " CYAN "%-16s" RESET " │", fn ? fn : ".text"));
        else
            cell(row, 2, lw, "             │ .text              │");
        row++;
        cell(row, 2, lw, "  0x08080000 └────────────────────┘"); row++;
        row++;
        cell(row, 2, lw, BOLD "  SRAM" RESET DIM " 128K" RESET); row++;
        cell(row, 2, lw, "  0x20000000 ┌────────────────────┐"); row++;
    }

    /* ── Task list (always show at bottom of left pane) ── */
    if (row < 24) row = 24;
    int num_tasks = *(uint32_t *)(ram);
    if (num_tasks > 0 && num_tasks <= 8) {
        cell(row, 2, lw, DIM "  Tasks:" RESET);
        row++;
        for (int t = 0; t < num_tasks; t++) {
            uint32_t sp = *(uint32_t *)(ram + 0x308 + t * 8);
            uint32_t name_ptr = *(uint32_t *)(ram + 0x308 + t * 8 + 4);
            char tname[12] = {0};
            if (name_ptr >= FLASH_BASE && name_ptr < FLASH_BASE + FLASH_SIZE) {
                const char *s = (const char *)(flash + (name_ptr - FLASH_BASE));
                int j;
                for (j = 0; j < 7 && s[j] >= 0x20 && s[j] < 0x7F; j++) tname[j] = s[j];
                tname[j] = '\0';
            }
            if (!tname[0]) { tname[0] = '0' + t; tname[1] = '\0'; }
            int active = (sp >= RAM_BASE && sp <= RAM_BASE + 0x2000 &&
                         (psp >= sp && psp <= sp + 256));
            if (active)
                cell(row, 2, lw, fmt("  " GREEN "▶ %-8s" RESET CYAN "%08X" RESET, tname, psp));
            else
                cell(row, 2, lw, fmt("    %-8s" DIM "%08X" RESET, tname, sp));
            row++;
        }
    }

    /* ── Right pane: UART console ── */
    int con_total = (con_count < CON_LINES) ? con_count + 1 : CON_LINES;
    int con_display = (con_total < 24) ? con_total : 24;
    int con_start_row = 3;
    for (int i = 0; i < con_display; i++)
        cell(con_start_row + i, rcol, rw, fmt(DIM "%s" RESET, con_line(con_display - 1 - i)));

    /* Footer */
    at(30, 2);
    fflush(out);
}
