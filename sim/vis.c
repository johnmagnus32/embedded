/*
 * vis.c — Two-pane visualizer: memory map (left) + console (right)
 */

#include <stdio.h>
#include <string.h>
#include "cpu.h"
#include "vis.h"

/* Console output buffer — captures UART writes */
#define CONSOLE_LINES 20
#define CONSOLE_WIDTH 30
static char console_buf[CONSOLE_LINES][CONSOLE_WIDTH + 1];
static int console_line = 0;
static int console_col = 0;
static int switch_count = 0;

void vis_console_putc(char c)
{
    if (c == '\r') {
        console_col = 0;
        return;
    }
    if (c == '\n' || console_col >= CONSOLE_WIDTH) {
        console_line = (console_line + 1) % CONSOLE_LINES;
        console_col = 0;
        memset(console_buf[console_line], ' ', CONSOLE_WIDTH);
        console_buf[console_line][CONSOLE_WIDTH] = '\0';
        if (c == '\n') return;
    }
    if (c == '\0') return;
    console_buf[console_line][console_col++] = c;
}

static const char *flag_str(uint32_t xpsr)
{
    static char buf[8];
    buf[0] = (xpsr & (1u << 31)) ? 'N' : '-';
    buf[1] = (xpsr & (1u << 30)) ? 'Z' : '-';
    buf[2] = (xpsr & (1u << 29)) ? 'C' : '-';
    buf[3] = (xpsr & (1u << 28)) ? 'V' : '-';
    buf[4] = '\0';
    return buf;
}

/* Print a line with left pane (40 chars) and right pane */
static void row(FILE *out, const char *left, int line_idx)
{
    /* Left pane */
    int len = strlen(left);
    fprintf(out, "%s", left);
    for (int i = len; i < 42; i++) fputc(' ', out);

    /* Separator */
    fprintf(out, "│ ");

    /* Right pane: console line */
    int idx = (console_line + 1 + line_idx) % CONSOLE_LINES;
    fprintf(out, "%s", console_buf[idx]);
    fprintf(out, "\n");
}

void vis_dump(FILE *out, struct cpu_state *cpu, uint8_t *flash, uint8_t *ram,
              const char *event)
{
    uint32_t pc = cpu->r[REG_PC];
    uint32_t psp = cpu->psp;
    uint32_t msp = cpu->msp;

    if (strstr(event, "PendSV")) switch_count++;

    /* Init console buffer on first call */
    static int inited = 0;
    if (!inited) {
        for (int i = 0; i < CONSOLE_LINES; i++) {
            memset(console_buf[i], ' ', CONSOLE_WIDTH);
            console_buf[i][CONSOLE_WIDTH] = '\0';
        }
        inited = 1;
    }

    fprintf(out, "\033[2J\033[H");

    /* Header */
    char hdr[80];
    snprintf(hdr, sizeof(hdr), "── %-28s Cycle: %-10llu ──",
             event, (unsigned long long)cpu->cycle_count);
    fprintf(out, "%s\n", hdr);

    char stats[80];
    snprintf(stats, sizeof(stats), "   Context switches: %d", switch_count);
    fprintf(out, "%s\n\n", stats);

    /* Column headers */
    fprintf(out, "  %-40s│ UART Console\n", "Memory Map");
    fprintf(out, "  %-40s│ ────────────────────────────\n", "");

    int ln = 0;

    /* Registers */
    char line[80];
    snprintf(line, sizeof(line), "  PC=0x%08X  Flags=%s  %s",
             pc, flag_str(cpu->xpsr), cpu->in_handler ? "[HANDLER]" : "[THREAD]");
    row(out, line, ln++);

    snprintf(line, sizeof(line), "  PSP=0x%08X  MSP=0x%08X", psp, msp);
    row(out, line, ln++);

    row(out, "", ln++);

    /* Flash */
    row(out, "  FLASH (512 KB)", ln++);
    row(out, "  0x08000000 ┌────────────────────────┐", ln++);
    row(out, "             │ .vector_table          │", ln++);
    row(out, "             ├────────────────────────┤", ln++);

    if (pc >= FLASH_BASE && pc < FLASH_BASE + FLASH_SIZE) {
        snprintf(line, sizeof(line), "         PC→ │ .text  0x%08X    │", pc);
        row(out, line, ln++);
    } else {
        row(out, "             │ .text                  │", ln++);
    }

    row(out, "             │ .rodata                │", ln++);
    row(out, "  0x08080000 └────────────────────────┘", ln++);
    row(out, "", ln++);

    /* SRAM */
    row(out, "  SRAM (128 KB)", ln++);
    row(out, "  0x20000000 ┌────────────────────────┐", ln++);
    row(out, "             │ .data + .bss           │", ln++);
    row(out, "             ├────────────────────────┤", ln++);

    /* Task stacks */
    int task_count = 0;
    for (uint32_t off = 0; off < 256 && task_count < 4; off += 4) {
        uint32_t val = *(uint32_t *)(ram + off);
        if (val >= RAM_BASE + 0x40 && val < RAM_BASE + 0x2000 && (val & 3) == 0) {
            int active = (val == psp);
            snprintf(line, sizeof(line), "             │ %s task%d SP=0x%08X │",
                     active ? "▶" : " ", task_count, val);
            row(out, line, ln++);
            task_count++;
        }
    }
    if (task_count == 0)
        row(out, "             │ (no tasks)             │", ln++);

    row(out, "             ├────────────────────────┤", ln++);
    row(out, "             │ Heap                   │", ln++);
    row(out, "             ├────────────────────────┤", ln++);

    snprintf(line, sizeof(line), "         MSP │ ISR stack         ↓    │ 0x%08X", msp);
    row(out, line, ln++);
    row(out, "  0x20020000 └────────────────────────┘", ln++);

    /* PC warning */
    if (pc < FLASH_BASE || pc >= FLASH_BASE + FLASH_SIZE) {
        row(out, "", ln++);
        snprintf(line, sizeof(line), "  ⚠ PC=0x%08X OUTSIDE flash!", pc);
        row(out, line, ln++);
    }

    fprintf(out, "\n");
}
