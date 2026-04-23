/*
 * vis.c — 2-panel debugger visualizer
 *
 * ┌──────────────────┬──────────────────┐
 * │  Source Code     │  Memory Map      │
 * │  (with → arrow)  │  (regs, flash,   │
 * │                  │   sram, tasks)   │
 * └──────────────────┴──────────────────┘
 *
 * UART output goes to /tmp/sim_uart fifo.
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

/* ── UART console (for non-debug vis mode) ── */
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

void vis_dbg_log(const char *fmt, ...) { (void)fmt; }

/* ── ANSI helpers ── */
#define ESC    "\033["
#define BOLD   ESC "1m"
#define DIM    ESC "2m"
#define GREEN  ESC "32m"
#define YELLOW ESC "33m"
#define CYAN   ESC "36m"
#define RESET  ESC "0m"

static FILE *g_out;
static int g_cols, g_rows, g_half;

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

/* Render a box content row: "0x%08X │%-*s│" with ANSI-aware padding */
#define BW 21
static char *box_row(uint32_t addr, int has_addr, const char *content)
{
    static char buf[256];
    char prefix[16];
    if (has_addr)
        snprintf(prefix, sizeof(prefix), "0x%08X ", addr);
    else
        snprintf(prefix, sizeof(prefix), "           ");

    /* Count visible chars in content */
    int vis = 0;
    for (const char *p = content; *p; p++) {
        if (*p == '\033') { while (*p && *p != 'm') p++; }
        else vis++;
    }
    int pad = BW - vis;
    if (pad < 0) pad = 0;

    char padding[64] = {0};
    for (int i = 0; i < pad && i < 63; i++) padding[i] = ' ';

    snprintf(buf, sizeof(buf), "%s│%s%s│", prefix, content, padding);
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
    g_half = g_cols / 2;
}

/* ── Source file cache ── */
static char *src_lines[4096];
static int src_nlines;
static char src_file[256];
static char src_search_dir[256];

void vis_set_source_dir(const char *elf_path)
{
    strncpy(src_search_dir, elf_path, sizeof(src_search_dir) - 1);
    char *slash = strrchr(src_search_dir, '/');
    if (slash) *(slash + 1) = '\0';
    else src_search_dir[0] = '\0';
}

static void load_source(const char *file)
{
    if (!file || strcmp(file, src_file) == 0) return;
    for (int i = 0; i < src_nlines; i++) free(src_lines[i]);
    src_nlines = 0;
    strncpy(src_file, file, sizeof(src_file) - 1);

    char path[512];
    FILE *f = NULL;
    if (src_search_dir[0]) {
        snprintf(path, sizeof(path), "%s%s", src_search_dir, file);
        f = fopen(path, "r");
        if (!f) { snprintf(path, sizeof(path), "%s../%s", src_search_dir, file); f = fopen(path, "r"); }
    }
    if (!f) f = fopen(file, "r");
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

void vis_dump(FILE *out, struct cpu_state *cpu, uint8_t *flash, uint8_t *ram,
              const char *event)
{
    g_out = out;
    get_size();
    int lw = g_half - 2;          /* left pane width (source) */
    int rw = g_cols - g_half - 3; /* right pane width (memory) */
    int rc = g_half + 3;          /* right pane col start */

    uint32_t pc  = cpu->r[REG_PC];
    uint32_t psp = cpu->psp;
    uint32_t msp = cpu->msp;
    int in_flash = (pc >= FLASH_BASE && pc < FLASH_BASE + FLASH_SIZE);

    if (strstr(event, "PendSV")) ctx_switches++;

    fprintf(out, ESC "2J" ESC "H" ESC "?25l");

    /* Vertical divider */
    for (int r = 1; r <= g_rows - 1; r++) {
        at(r, g_half + 1); fprintf(out, DIM "│" RESET);
    }

    uint32_t sym_off;
    const char *fn = sym_lookup(pc, &sym_off);

    /* ════════════════════════════════════════════
     * LEFT PANEL: Source Code
     * ════════════════════════════════════════════ */
    int cur_line;
    const char *file = line_lookup(pc, &cur_line);
    if (file) load_source(file);

    int src_row = 1;
    /* Header: function + event */
    cell(src_row, 2, lw, fmt(BOLD "%s" RESET DIM "  cy %-7llu ctx %d" RESET,
         event, (unsigned long long)cpu->cycle_count, ctx_switches));
    src_row++;

    if (src_nlines > 0 && cur_line > 0) {
        cell(src_row, 2, lw, fmt(DIM "%s" RESET, file));
        src_row++;

        int avail = g_rows - 4;
        int context = avail / 2;
        int start = cur_line - context;
        if (start < 1) start = 1;
        int end = start + avail - 1;
        if (end > src_nlines) end = src_nlines;

        for (int l = start; l <= end; l++) {
            const char *lt = (l <= src_nlines) ? src_lines[l - 1] : "";
            char tr[128];
            int maxw = lw - 6;
            if (maxw > 120) maxw = 120;
            strncpy(tr, lt, maxw); tr[maxw] = '\0';
            for (char *p = tr; *p; p++) if (*p == '\t') *p = ' ';
            if (l == cur_line)
                cell(src_row, 2, lw, fmt(CYAN "→%3d" RESET " " BOLD "%s" RESET, l, tr));
            else
                cell(src_row, 2, lw, fmt(DIM " %3d" RESET " %s", l, tr));
            src_row++;
        }
    } else {
        cell(src_row, 2, lw, DIM "  (no source — compile with -g)" RESET);
    }

    /* ════════════════════════════════════════════
     * RIGHT PANEL: Memory Map
     * ════════════════════════════════════════════ */
    int row = 1;
    #define BW 21  /* box inner width */

    /* Helper: render "addr │content padded to BW│" */
    #define BOX_TOP(addr)       cell(row, rc, rw, fmt("0x%08X ┌" "─────────────────────" "┐", addr)); row++
    #define BOX_BOT(addr)       cell(row, rc, rw, fmt("0x%08X └" "─────────────────────" "┘", addr)); row++
    #define BOX_SEP(addr)       cell(row, rc, rw, fmt("0x%08X ├" "╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌" "┤", addr)); row++
    /* For box rows with content: we print addr+│, then content, then pad+│ */
    /* box_content: prints a row with address, opening │, content (with ANSI), closing │ */

    /* Registers */
    cell(row, rc, rw, fmt("PC " CYAN "%08X" RESET " %s",
         pc, cpu->in_handler ? YELLOW "[HANDLER]" RESET : GREEN "[THREAD]" RESET));
    row++;
    cell(row, rc, rw, fmt(GREEN "%s" RESET "+" CYAN "0x%X" RESET, fn ? fn : "???", sym_off));
    row++;
    cell(row, rc, rw, fmt("PSP " CYAN "%08X" RESET " MSP " CYAN "%08X" RESET, psp, msp));
    row += 2;

    /* Flash */
    BOX_TOP(FLASH_BASE);
    if (in_flash) {
        cell(row, rc, rw, box_row(0, 0, fmt(CYAN " PC →" RESET " %-13s", fn ? fn : ".text"))); row++;
    } else {
        cell(row, rc, rw, box_row(0, 0, " .text")); row++;
    }
    BOX_BOT(FLASH_BASE + FLASH_SIZE);

    /* SRAM */
    BOX_TOP(RAM_BASE);
    cell(row, rc, rw, box_row(0, 0, " .data + .bss")); row++;

    /* Tasks */
    uint32_t sym_nt = sym_find_by_name("num_tasks");
    uint32_t sym_tasks = sym_find_by_name("tasks");
    uint32_t sym_stacks = sym_find_by_name("task_stacks");
    if (!sym_stacks) sym_stacks = sym_find_by_name("stacks");
    if (sym_nt && sym_tasks && sym_stacks) {
        int tcb_size, stk_size;
        if (sym_find_by_name("task_stacks")) { tcb_size = 32; stk_size = 512; }
        else { tcb_size = 8; stk_size = 256; }
        uint32_t nt_off = sym_nt - RAM_BASE;
        uint32_t tasks_off = sym_tasks - RAM_BASE;
        uint32_t stk_base = sym_stacks - RAM_BASE;
        int num_tasks = *(uint32_t *)(ram + nt_off);
        if (num_tasks > 0 && num_tasks <= 8) {
            for (int t = 0; t < num_tasks && row < g_rows - 5; t++) {
                uint32_t sp = *(uint32_t *)(ram + tasks_off + t * tcb_size);
                uint32_t name_ptr = *(uint32_t *)(ram + tasks_off + t * tcb_size + 4);
                char tn[12] = {0};
                if (name_ptr >= FLASH_BASE && name_ptr < FLASH_BASE + FLASH_SIZE) {
                    const char *s = (const char *)(flash + (name_ptr - FLASH_BASE));
                    for (int j = 0; j < 7 && s[j] >= 0x20 && s[j] < 0x7F; j++) tn[j] = s[j];
                }
                if (!tn[0]) { tn[0] = '0' + t; tn[1] = '\0'; }
                uint32_t stk_top = RAM_BASE + stk_base + (t + 1) * stk_size;
                uint32_t stk_bot = stk_top - stk_size;
                int active = (psp >= stk_bot && psp <= stk_top);
                uint32_t dsp = active ? psp : sp;
                int used = (dsp >= stk_bot && dsp <= stk_top) ? (int)(stk_top - dsp) : 0;
                BOX_SEP(stk_bot);
                cell(row, rc, rw, box_row(dsp, 1, fmt(CYAN " SP" RESET " %s%s%s %3d/%-3d",
                    active ? GREEN : "", active ? "▶" : " ", active ? RESET : "", used, stk_size))); row++;
                cell(row, rc, rw, box_row(0, 0, fmt(" %s%s%s", active ? GREEN : "", tn, active ? RESET : ""))); row++;
            }
            BOX_SEP(RAM_BASE + stk_base + num_tasks * stk_size);
        }
    }
    cell(row, rc, rw, box_row(0, 0, " heap")); row++;
    cell(row, rc, rw, box_row(msp, 1, YELLOW " MSP" RESET)); row++;
    BOX_BOT(RAM_BASE + RAM_SIZE);

    /* Cursor at bottom for prompt */
    at(g_rows, 1);
    fprintf(out, ESC "?25h");
    fflush(out);
}
