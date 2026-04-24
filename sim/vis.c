/*
 * vis.c — Debugger source code display
 *
 * Shows source code with → arrow at current line.
 * Memory map is handled externally via --state JSON file.
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

/* ── UART console (kept for non-debug mode) ── */
void vis_console_putc(char c) { (void)c; }
void vis_dbg_log(const char *fmt, ...) { (void)fmt; }

/* ── ANSI helpers ── */
#define ESC    "\033["
#define BOLD   ESC "1m"
#define DIM    ESC "2m"
#define GREEN  ESC "32m"
#define YELLOW ESC "33m"
#define CYAN   ESC "36m"
#define RESET  ESC "0m"

static int get_rows(void)
{
    struct winsize w;
    if (ioctl(STDERR_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_row > 10)
        return w.ws_row;
    return 40;
}

static int get_cols(void)
{
    struct winsize w;
    if (ioctl(STDERR_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 20)
        return w.ws_col;
    return 80;
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
    (void)flash; (void)ram;
    int rows = get_rows();
    int cols = get_cols();
    uint32_t pc = cpu->r[REG_PC];

    uint32_t sym_off;
    const char *fn = sym_lookup(pc, &sym_off);

    fprintf(out, ESC "2J" ESC "H" ESC "?25l");

    int row = 1;

    /* Header */
    fprintf(out, ESC "%d;1H", row);
    fprintf(out, BOLD "%s" RESET "  " DIM "cy %llu" RESET "  "
            "PC " CYAN "%08X" RESET " %s " GREEN "%s" RESET "+" CYAN "0x%X" RESET
            "  PSP " CYAN "%08X" RESET " MSP " CYAN "%08X" RESET,
            event, (unsigned long long)cpu->cycle_count,
            pc, cpu->in_handler ? YELLOW "[H]" RESET : "[T]",
            fn ? fn : "???", sym_off, cpu->psp, cpu->msp);
    row += 2;

    /* Source code */
    int cur_line;
    const char *file = line_lookup(pc, &cur_line);
    if (file) load_source(file);

    if (src_nlines > 0 && cur_line > 0) {
        fprintf(out, ESC "%d;1H" DIM "%s" RESET, row, file);
        row++;

        int avail = rows - row - 1;
        int context = avail / 2;
        int start = cur_line - context;
        if (start < 1) start = 1;
        int end = start + avail - 1;
        if (end > src_nlines) end = src_nlines;

        for (int l = start; l <= end; l++) {
            const char *lt = src_lines[l - 1];
            /* Truncate to terminal width */
            char tr[256];
            int maxw = cols - 6;
            if (maxw > 250) maxw = 250;
            strncpy(tr, lt, maxw); tr[maxw] = '\0';
            for (char *p = tr; *p; p++) if (*p == '\t') *p = ' ';

            fprintf(out, ESC "%d;1H", row);
            if (l == cur_line)
                fprintf(out, CYAN "→%3d" RESET " " BOLD "%s" RESET, l, tr);
            else
                fprintf(out, DIM " %3d" RESET " %s", l, tr);
            /* Clear to end of line */
            fprintf(out, ESC "K");
            row++;
        }
    } else {
        fprintf(out, ESC "%d;1H" DIM "(no source — compile with -g)" RESET, row);
    }

    /* Cursor at bottom for prompt */
    fprintf(out, ESC "%d;1H" ESC "?25h", rows);
    fflush(out);
}
