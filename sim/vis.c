/*
 * vis.c — ASCII state visualizer for the ARM emulator
 */

#include <stdio.h>
#include <string.h>
#include "cpu.h"
#include "vis.h"

/* Read a 32-bit value from RAM (helper) */
static uint32_t ram32(uint8_t *ram, uint32_t addr)
{
    if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE)
        return *(uint32_t *)(ram + (addr - RAM_BASE));
    return 0;
}

/* Try to detect task info from RAM by scanning for known patterns.
 * The test_rtos.c firmware stores tasks at a known BSS location.
 * We look for the 'tasks' array and 'current_task' variable.
 */

#define MAX_VIS_TASKS 8

struct vis_task {
    uint32_t sp;
    uint32_t stack_base;  /* estimated */
    int active;
    char name[8];
};

/* Scan RAM for task stacks by looking at SP values in a plausible range */
static int find_tasks(uint8_t *ram, struct vis_task *vt, uint32_t current_sp)
{
    /* Heuristic: scan BSS area for pointers that look like stack pointers
     * (values in 0x20000000-0x20020000 range, aligned to 4) */
    int count = 0;

    /* For the test_rtos firmware, tasks[] is near the start of BSS.
     * Each TCB is just a uint32_t* (4 bytes). We scan the first 256 bytes. */
    for (uint32_t off = 0; off < 256 && count < MAX_VIS_TASKS; off += 4) {
        uint32_t val = *(uint32_t *)(ram + off);
        if (val >= RAM_BASE && val < RAM_BASE + RAM_SIZE && (val & 3) == 0) {
            vt[count].sp = val;
            vt[count].stack_base = val & ~0xFF;  /* rough estimate */
            vt[count].active = (val == current_sp) ? 1 : 0;
            snprintf(vt[count].name, sizeof(vt[count].name), "task%d", count);
            count++;
        }
    }
    return count;
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

static void bar(FILE *out, int width, char fill)
{
    for (int i = 0; i < width; i++) fputc(fill, out);
}

void vis_dump(FILE *out, struct cpu_state *cpu, uint8_t *flash, uint8_t *ram)
{
    uint32_t pc = cpu->r[REG_PC];
    uint32_t sp = cpu->r[REG_SP];
    uint32_t psp = cpu->psp;
    uint32_t msp = cpu->msp;

    fprintf(out, "\033[2J\033[H");  /* clear screen */

    /* Header */
    fprintf(out, "╔══════════════════════════════════════════════════════════╗\n");
    fprintf(out, "║  ARM Cortex-M4 Emulator — Cycle %llu                    \n",
            (unsigned long long)cpu->cycle_count);
    fprintf(out, "╠══════════════════════════════════════════════════════════╣\n");

    /* Registers */
    fprintf(out, "║ PC=0x%08X  SP=0x%08X  LR=0x%08X          ║\n",
            pc, sp, cpu->r[REG_LR]);
    fprintf(out, "║ PSP=0x%08X MSP=0x%08X Flags=%s %s     ║\n",
            psp, msp, flag_str(cpu->xpsr),
            cpu->in_handler ? "[HANDLER]" : "[THREAD] ");
    fprintf(out, "║ r0=%08X r1=%08X r2=%08X r3=%08X      ║\n",
            cpu->r[0], cpu->r[1], cpu->r[2], cpu->r[3]);
    fprintf(out, "╠══════════════════════════════════════════════════════════╣\n");

    /* Memory map */
    fprintf(out, "║                    MEMORY MAP                           ║\n");
    fprintf(out, "║                                                         ║\n");

    /* Flash */
    fprintf(out, "║ FLASH 0x08000000 ");
    bar(out, 38, '█');
    fprintf(out, "  ║\n");

    /* Show PC position in flash */
    if (pc >= FLASH_BASE && pc < FLASH_BASE + FLASH_SIZE) {
        int pos = (pc - FLASH_BASE) * 38 / FLASH_SIZE;
        fprintf(out, "║       PC→");
        for (int i = 0; i < pos + 8; i++) fputc(' ', out);
        fprintf(out, "▲ 0x%08X", pc);
        int pad = 38 - pos - 14;
        if (pad > 0) for (int i = 0; i < pad; i++) fputc(' ', out);
        fprintf(out, "  ║\n");
    }

    fprintf(out, "║                                                         ║\n");

    /* RAM — show regions */
    fprintf(out, "║ RAM   0x20000000 ");
    bar(out, 38, '░');
    fprintf(out, "  ║\n");

    /* Detect tasks and show their stacks */
    struct vis_task tasks[MAX_VIS_TASKS];
    int ntasks = find_tasks(ram, tasks, psp);

    /* Show stack regions */
    for (int i = 0; i < ntasks && i < 4; i++) {
        uint32_t tsp = tasks[i].sp;
        int pos = 0;
        if (tsp >= RAM_BASE && tsp < RAM_BASE + RAM_SIZE)
            pos = (tsp - RAM_BASE) * 38 / RAM_SIZE;

        if (tasks[i].active)
            fprintf(out, "║  ★ %s SP→", tasks[i].name);
        else
            fprintf(out, "║    %s SP→", tasks[i].name);

        for (int j = 0; j < pos; j++) fputc(' ', out);
        fprintf(out, "▲ 0x%08X", tsp);
        int pad = 38 - pos - 14 + (8 - (int)strlen(tasks[i].name));
        if (pad > 0) for (int j = 0; j < pad; j++) fputc(' ', out);
        fprintf(out, "  ║\n");
    }

    /* MSP */
    {
        int pos = (msp >= RAM_BASE) ? (msp - RAM_BASE) * 38 / RAM_SIZE : 37;
        fprintf(out, "║    MSP  →");
        for (int i = 0; i < pos; i++) fputc(' ', out);
        fprintf(out, "▲ 0x%08X", msp);
        int pad = 38 - pos - 14 + 8;
        if (pad > 0) for (int i = 0; i < pad; i++) fputc(' ', out);
        fprintf(out, "  ║\n");
    }

    fprintf(out, "║                                                         ║\n");

    /* Task list */
    fprintf(out, "╠══════════════════════════════════════════════════════════╣\n");
    fprintf(out, "║ TASKS (%d found)                                        ║\n", ntasks);
    for (int i = 0; i < ntasks; i++) {
        fprintf(out, "║  %s %-6s  SP=0x%08X  %s                    ║\n",
                tasks[i].active ? "▶" : " ",
                tasks[i].name,
                tasks[i].sp,
                tasks[i].active ? "RUNNING" : "ready  ");
    }

    fprintf(out, "╚══════════════════════════════════════════════════════════╝\n");
}
