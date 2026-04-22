/*
 * vis.c — Two-pane visualizer using ANSI escape codes
 *
 * Uses exact cursor positioning so columns line up perfectly.
 * Left pane: memory map. Right pane: UART console.
 */

#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "cpu.h"
#include "vis.h"

/* Console ring buffer */
#define CON_LINES 24
#define CON_WIDTH 36
static char con[CON_LINES][CON_WIDTH + 1];
static int con_row = 0;
static int con_col = 0;
static int ctx_switches = 0;

void vis_console_putc(char c)
{
    if (c == '\r') { con_col = 0; return; }
    if (c == '\n' || con_col >= CON_WIDTH) {
        con_row = (con_row + 1) % CON_LINES;
        con_col = 0;
        memset(con[con_row], 0, CON_WIDTH + 1);
        if (c == '\n') return;
    }
    if (c >= 32) con[con_row][con_col++] = c;
}

/* ANSI helpers */
#define ESC "\033["
#define CLEAR ESC "2J" ESC "H"
#define BOLD ESC "1m"
#define DIM ESC "2m"
#define GREEN ESC "32m"
#define YELLOW ESC "33m"
#define CYAN ESC "36m"
#define RED ESC "31m"
#define RESET ESC "0m"
#define REVERSE ESC "7m"

/* Move cursor to row, col (1-based) */
static void moveto(FILE *f, int row, int col) { fprintf(f, ESC "%d;%dH", row, col); }

/* Print at exact position, padded to width */
static void printat(FILE *f, int row, int col, int width, const char *fmt, ...)
{
    moveto(f, row, col);
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    fprintf(f, "%s", buf);
    for (int i = n; i < width; i++) fputc(' ', f);
}

#include <stdarg.h>

#define LEFT 2       /* left column start */
#define MID 44       /* divider column */
#define RIGHT 46     /* right column start */
#define WIDTH_L 40   /* left pane width */
#define WIDTH_R 36   /* right pane width */

static void draw_divider(FILE *f, int rows)
{
    for (int r = 1; r <= rows; r++) {
        moveto(f, r, MID);
        fprintf(f, DIM "│" RESET);
    }
}

void vis_dump(FILE *out, struct cpu_state *cpu, uint8_t *flash, uint8_t *ram,
              const char *event)
{
    uint32_t pc = cpu->r[REG_PC];
    uint32_t psp = cpu->psp;
    uint32_t msp = cpu->msp;

    if (strstr(event, "PendSV")) ctx_switches++;

    static int inited = 0;
    if (!inited) {
        for (int i = 0; i < CON_LINES; i++) memset(con[i], 0, CON_WIDTH + 1);
        inited = 1;
    }

    fprintf(out, CLEAR);
    int row = 1;

    /* ── Header ── */
    printat(out, row, LEFT, 80, BOLD "%-30s" RESET DIM "  cycle %llu  ctx_sw %d" RESET,
            event, (unsigned long long)cpu->cycle_count, ctx_switches);
    row += 2;

    /* ── Column headers ── */
    printat(out, row, LEFT, WIDTH_L, BOLD "  Memory Map" RESET);
    printat(out, row, RIGHT, WIDTH_R, BOLD "UART Console" RESET);
    row++;

    /* Draw divider for all rows */
    draw_divider(out, 35);

    /* ── Registers ── */
    int pc_in_flash = (pc >= FLASH_BASE && pc < FLASH_BASE + FLASH_SIZE);
    printat(out, row, LEFT, WIDTH_L, "  PC=" CYAN "0x%08X" RESET "  %s  %s",
            pc, cpu->in_handler ? YELLOW "[HANDLER]" RESET : GREEN "[THREAD]" RESET,
            pc_in_flash ? "" : RED "⚠" RESET);
    /* Right pane: console line */
    int cl = 0;
    printat(out, row, RIGHT, WIDTH_R, DIM "%s" RESET, con[(con_row + 1 + cl++) % CON_LINES]);
    row++;

    printat(out, row, LEFT, WIDTH_L, "  PSP=" CYAN "0x%08X" RESET "  MSP=" CYAN "0x%08X" RESET, psp, msp);
    printat(out, row, RIGHT, WIDTH_R, DIM "%s" RESET, con[(con_row + 1 + cl++) % CON_LINES]);
    row += 2;

    /* ── Flash ── */
    printat(out, row, LEFT, WIDTH_L, BOLD "  FLASH" RESET DIM " (512 KB)" RESET);
    printat(out, row, RIGHT, WIDTH_R, DIM "%s" RESET, con[(con_row + 1 + cl++) % CON_LINES]);
    row++;

    printat(out, row, LEFT, WIDTH_L, "  0x08000000 ┌──────────────────────┐");
    printat(out, row, RIGHT, WIDTH_R, DIM "%s" RESET, con[(con_row + 1 + cl++) % CON_LINES]);
    row++;

    printat(out, row, LEFT, WIDTH_L, "             │ .vector_table        │");
    printat(out, row, RIGHT, WIDTH_R, DIM "%s" RESET, con[(con_row + 1 + cl++) % CON_LINES]);
    row++;

    printat(out, row, LEFT, WIDTH_L, "             ├──────────────────────┤");
    printat(out, row, RIGHT, WIDTH_R, DIM "%s" RESET, con[(con_row + 1 + cl++) % CON_LINES]);
    row++;

    if (pc_in_flash) {
        printat(out, row, LEFT, WIDTH_L, "  " CYAN "PC →" RESET "     │ .text " CYAN "0x%08X" RESET "  │", pc);
    } else {
        printat(out, row, LEFT, WIDTH_L, "             │ .text                │");
    }
    printat(out, row, RIGHT, WIDTH_R, DIM "%s" RESET, con[(con_row + 1 + cl++) % CON_LINES]);
    row++;

    printat(out, row, LEFT, WIDTH_L, "             │ .rodata              │");
    printat(out, row, RIGHT, WIDTH_R, DIM "%s" RESET, con[(con_row + 1 + cl++) % CON_LINES]);
    row++;

    printat(out, row, LEFT, WIDTH_L, "  0x08080000 └──────────────────────┘");
    printat(out, row, RIGHT, WIDTH_R, DIM "%s" RESET, con[(con_row + 1 + cl++) % CON_LINES]);
    row += 2;

    /* ── SRAM ── */
    printat(out, row, LEFT, WIDTH_L, BOLD "  SRAM" RESET DIM " (128 KB)" RESET);
    printat(out, row, RIGHT, WIDTH_R, DIM "%s" RESET, con[(con_row + 1 + cl++) % CON_LINES]);
    row++;

    printat(out, row, LEFT, WIDTH_L, "  0x20000000 ┌──────────────────────┐");
    printat(out, row, RIGHT, WIDTH_R, DIM "%s" RESET, con[(con_row + 1 + cl++) % CON_LINES]);
    row++;

    printat(out, row, LEFT, WIDTH_L, "             │ .data + .bss         │");
    printat(out, row, RIGHT, WIDTH_R, DIM "%s" RESET, con[(con_row + 1 + cl++) % CON_LINES]);
    row++;

    printat(out, row, LEFT, WIDTH_L, "             ├──────────────────────┤");
    printat(out, row, RIGHT, WIDTH_R, DIM "%s" RESET, con[(con_row + 1 + cl++) % CON_LINES]);
    row++;

    /* Tasks */
    int tc = 0;
    for (uint32_t off = 0; off < 256 && tc < 4; off += 4) {
        uint32_t val = *(uint32_t *)(ram + off);
        if (val >= RAM_BASE + 0x40 && val < RAM_BASE + 0x2000 && (val & 3) == 0) {
            int active = (val == psp);
            if (active)
                printat(out, row, LEFT, WIDTH_L,
                        "             │ " GREEN "▶ task%d" RESET " SP=" CYAN "0x%08X" RESET " │", tc, val);
            else
                printat(out, row, LEFT, WIDTH_L,
                        "             │   task%d SP=" DIM "0x%08X" RESET " │", tc, val);
            printat(out, row, RIGHT, WIDTH_R, DIM "%s" RESET, con[(con_row + 1 + cl++) % CON_LINES]);
            row++;
            tc++;
        }
    }
    if (tc == 0) {
        printat(out, row, LEFT, WIDTH_L, "             │ " DIM "(no tasks)" RESET "           │");
        printat(out, row, RIGHT, WIDTH_R, DIM "%s" RESET, con[(con_row + 1 + cl++) % CON_LINES]);
        row++;
    }

    printat(out, row, LEFT, WIDTH_L, "             ├──────────────────────┤");
    printat(out, row, RIGHT, WIDTH_R, DIM "%s" RESET, con[(con_row + 1 + cl++) % CON_LINES]);
    row++;

    printat(out, row, LEFT, WIDTH_L, "             │ Heap                 │");
    printat(out, row, RIGHT, WIDTH_R, DIM "%s" RESET, con[(con_row + 1 + cl++) % CON_LINES]);
    row++;

    printat(out, row, LEFT, WIDTH_L, "             ├──────────────────────┤");
    printat(out, row, RIGHT, WIDTH_R, DIM "%s" RESET, con[(con_row + 1 + cl++) % CON_LINES]);
    row++;

    printat(out, row, LEFT, WIDTH_L, "  " YELLOW "MSP →" RESET "    │ ISR stack       ↓    │ " DIM "0x%08X" RESET, msp);
    printat(out, row, RIGHT, WIDTH_R, DIM "%s" RESET, con[(con_row + 1 + cl++) % CON_LINES]);
    row++;

    printat(out, row, LEFT, WIDTH_L, "  0x20020000 └──────────────────────┘");
    printat(out, row, RIGHT, WIDTH_R, DIM "%s" RESET, con[(con_row + 1 + cl++) % CON_LINES]);
    row += 2;

    if (!pc_in_flash) {
        printat(out, row, LEFT, 80, RED "  ⚠ PC=0x%08X is OUTSIDE flash!" RESET, pc);
        row++;
    }

    moveto(out, row, LEFT);
    fflush(out);
}
