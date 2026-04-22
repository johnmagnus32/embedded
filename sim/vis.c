/*
 * vis.c — Two-pane visualizer for the ARM Cortex-M4 emulator
 *
 * Splits the terminal exactly in half. Left pane: CPU/memory state.
 * Right pane: UART console output. All positioning uses exact cursor
 * moves so ANSI color codes don't affect alignment.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "cpu.h"
#include "vis.h"

/* ── UART console ring buffer ── */
#define CON_LINES 32
#define CON_WIDTH 60
static char con[CON_LINES][CON_WIDTH + 1];
static int con_row = 0;
static int con_col = 0;
static int con_count = 0;   /* total lines written */
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
static int g_cols;   /* terminal width */
static int g_half;   /* left pane width (cols/2) */

static void at(int row, int col) { fprintf(g_out, ESC "%d;%dH", row, col); }

/* Print plain text at (row, col), padded to exactly `w` visible chars */
static void cell(int row, int col, int w, const char *s)
{
    at(row, col);
    int vis = 0;
    /* Count visible chars (skip ANSI sequences) */
    for (const char *p = s; *p; p++) {
        if (*p == '\033') { while (*p && *p != 'm') p++; }
        else vis++;
    }
    fprintf(g_out, "%s", s);
    for (int i = vis; i < w; i++) fputc(' ', g_out);
}

/* snprintf into a static buffer and return it */
static char *fmt(const char *f, ...)
{
    static char buf[256];
    va_list ap;
    va_start(ap, f);
    vsnprintf(buf, sizeof(buf), f, ap);
    va_end(ap);
    return buf;
}

/* Get the i-th most recent console line (0 = newest) */
static const char *con_line(int i)
{
    int idx = (con_row - i + CON_LINES) % CON_LINES;
    return con[idx];
}

static int get_cols(void)
{
    struct winsize w;
    if (ioctl(STDERR_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 40)
        return w.ws_col;
    return 80;
}

void vis_dump(FILE *out, struct cpu_state *cpu, uint8_t *flash, uint8_t *ram,
              const char *event)
{
    g_out = out;
    g_cols = get_cols();
    g_half = g_cols / 2;
    int lw = g_half - 2;          /* left pane usable width */
    int rw = g_cols - g_half - 3; /* right pane usable width */
    int rcol = g_half + 3;        /* right pane text start */

    uint32_t pc  = cpu->r[REG_PC];
    uint32_t psp = cpu->psp;
    uint32_t msp = cpu->msp;
    int in_flash = (pc >= FLASH_BASE && pc < FLASH_BASE + FLASH_SIZE);

    if (strstr(event, "PendSV")) ctx_switches++;

    fprintf(out, CLEAR);

    /* ── Draw vertical divider ── */
    for (int r = 1; r <= 30; r++) {
        at(r, g_half + 1);
        fprintf(out, DIM "│" RESET);
    }

    /* Console line index — show newest at bottom, oldest at top */
    int con_total = (con_count < CON_LINES) ? con_count + 1 : CON_LINES;
    int con_display = (con_total < 20) ? con_total : 20;

    int row = 1;

    /* ── Header ── */
    cell(row, 2, lw, fmt(BOLD "%-28s" RESET DIM "  cy %-8llu ctx %d" RESET,
         event, (unsigned long long)cpu->cycle_count, ctx_switches));
    cell(row, rcol, rw, BOLD "UART Console" RESET);
    row++;

    /* ── Registers ── */
    row++;
    cell(row, 2, lw, fmt("  PC  " CYAN "0x%08X" RESET "  %s",
         pc, cpu->in_handler ? YELLOW "[HANDLER]" RESET : GREEN "[THREAD]" RESET));
    row++;
    cell(row, 2, lw, fmt("  PSP " CYAN "0x%08X" RESET "  MSP " CYAN "0x%08X" RESET, psp, msp));
    row++;

    /* ── Flash ── */
    row++;
    cell(row, 2, lw, BOLD "  FLASH" RESET DIM " 512K" RESET);
    row++;
    cell(row, 2, lw, "  0x08000000 ┌────────────────────┐");
    row++;
    cell(row, 2, lw, "             │ .vectors           │");
    row++;
    cell(row, 2, lw, "             ├────────────────────┤");
    row++;
    if (in_flash)
        cell(row, 2, lw, fmt("  " CYAN "PC →" RESET "     │ .text " CYAN "0x%08X" RESET " │", pc));
    else
        cell(row, 2, lw, "             │ .text              │");
    row++;
    cell(row, 2, lw, "             │ .rodata            │");
    row++;
    cell(row, 2, lw, "  0x08080000 └────────────────────┘");
    row++;

    /* ── SRAM ── */
    row++;
    cell(row, 2, lw, BOLD "  SRAM" RESET DIM " 128K" RESET);
    row++;
    cell(row, 2, lw, "  0x20000000 ┌────────────────────┐");
    row++;
    cell(row, 2, lw, "             │ .data + .bss       │");
    row++;
    cell(row, 2, lw, "             ├────────────────────┤");
    row++;

    /* Task stacks — scan the task SP table in RAM */
    int num_tasks = *(uint32_t *)(ram);  /* num_tasks at offset 0 */
    if (num_tasks > 0 && num_tasks <= 8) {
        for (int t = 0; t < num_tasks; t++) {
            /* tasks[t].sp is at RAM offset 0x308 + t*4 */
            uint32_t sp = *(uint32_t *)(ram + 0x308 + t * 4);
            int active = (sp >= RAM_BASE && sp <= RAM_BASE + 0x2000 &&
                         (psp >= sp && psp <= sp + 256));
            if (active)
                cell(row, 2, lw, fmt("  " CYAN "PSP→" RESET "     │ " GREEN "▶task%d" RESET " " CYAN "%08X" RESET "  │", t, psp));
            else
                cell(row, 2, lw, fmt("             │  task%d " DIM "%08X" RESET "  │", t, sp));
            row++;
        }
    } else {
        cell(row, 2, lw, "             │ " DIM "(no tasks)" RESET "        │");
        row++;
    }

    cell(row, 2, lw, "             ├────────────────────┤");
    row++;
    cell(row, 2, lw, "             │ Heap               │");
    row++;
    cell(row, 2, lw, "             ├────────────────────┤");
    row++;
    cell(row, 2, lw, fmt("  " YELLOW "MSP →" RESET "    │ ISR stack    ↓     │ " DIM "%08X" RESET, msp));
    row++;
    cell(row, 2, lw, "  0x20020000 └────────────────────┘");
    row++;

    /* ── Right pane: UART console (newest lines at bottom) ── */
    int con_start_row = 3;  /* first row for console text */
    for (int i = 0; i < con_display; i++) {
        const char *line = con_line(con_display - 1 - i);
        cell(con_start_row + i, rcol, rw, fmt(DIM "%s" RESET, line));
    }

    /* ── Footer ── */
    row += 1;
    at(row, 2);
    fprintf(out, DIM "  [Enter] step" RESET);
    fflush(out);
}
