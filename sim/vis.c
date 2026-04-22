/*
 * vis.c — Event-driven state visualizer
 *
 * Only updates on significant events (interrupt, context switch, etc.)
 * Shows the flash/SRAM memory map with current PC and SP positions.
 */

#include <stdio.h>
#include <string.h>
#include "cpu.h"
#include "vis.h"

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

void vis_dump(FILE *out, struct cpu_state *cpu, uint8_t *flash, uint8_t *ram,
              const char *event)
{
    uint32_t pc = cpu->r[REG_PC];
    uint32_t psp = cpu->psp;
    uint32_t msp = cpu->msp;

    fprintf(out, "\033[2J\033[H");

    /* Event and cycle */
    fprintf(out, "── Event: %-30s  Cycle: %llu ──\n\n",
            event, (unsigned long long)cpu->cycle_count);

    /* Registers */
    fprintf(out, "  PC=0x%08X  LR=0x%08X  Flags=%s  %s\n",
            pc, cpu->r[REG_LR], flag_str(cpu->xpsr),
            cpu->in_handler ? "[HANDLER]" : "[THREAD]");
    fprintf(out, "  PSP=0x%08X  MSP=0x%08X  PRIMASK=%d\n\n",
            psp, msp, cpu->primask);

    /* Flash map */
    fprintf(out, "  FLASH (512 KB)\n");
    fprintf(out, "  0x08000000 ┌──────────────────────────┐\n");
    fprintf(out, "             │ .vector_table            │\n");
    fprintf(out, "             ├──────────────────────────┤\n");

    /* Show PC position in .text */
    if (pc >= FLASH_BASE && pc < FLASH_BASE + FLASH_SIZE) {
        fprintf(out, "             │ .text                    │\n");
        fprintf(out, "         PC→ │  0x%08X              │\n", pc);
        fprintf(out, "             │                          │\n");
    } else {
        fprintf(out, "             │ .text                    │\n");
        fprintf(out, "             │                          │\n");
        fprintf(out, "             │                          │\n");
    }

    fprintf(out, "             ├──────────────────────────┤\n");
    fprintf(out, "             │ .rodata                  │\n");
    fprintf(out, "             ├──────────────────────────┤\n");
    fprintf(out, "             │ .data init values        │\n");
    fprintf(out, "  0x08080000 └──────────────────────────┘\n\n");

    /* SRAM map */
    fprintf(out, "  SRAM (128 KB)\n");
    fprintf(out, "  0x20000000 ┌──────────────────────────┐\n");
    fprintf(out, "             │ .data (globals)          │\n");
    fprintf(out, "             ├──────────────────────────┤\n");
    fprintf(out, "             │ .bss (zeroed globals)    │\n");
    fprintf(out, "             ├──────────────────────────┤\n");

    /* Task stacks — scan RAM for plausible stack pointers */
    fprintf(out, "             │ Task stacks              │\n");

    /* Heuristic: look at first few words of BSS for task SP values */
    int task_count = 0;
    for (uint32_t off = 0; off < 256 && task_count < 6; off += 4) {
        uint32_t val = *(uint32_t *)(ram + off);
        if (val >= RAM_BASE + 0x40 && val < RAM_BASE + 0x2000 && (val & 3) == 0) {
            int is_active = (val == psp);
            fprintf(out, "             │  %s task %d  SP=0x%08X │\n",
                    is_active ? "▶" : " ", task_count, val);
            task_count++;
        }
    }

    if (task_count == 0) {
        fprintf(out, "             │  (no tasks detected)     │\n");
    }

    fprintf(out, "             ├──────────────────────────┤\n");
    fprintf(out, "             │ Heap                     │\n");
    fprintf(out, "             │                          │\n");
    fprintf(out, "             ├──────────────────────────┤\n");

    /* MSP position */
    fprintf(out, "         MSP │ ISR stack (1 KB)    ↓    │ 0x%08X\n", msp);
    fprintf(out, "  0x20020000 └──────────────────────────┘\n\n");

    /* PC outside flash warning */
    if (pc < FLASH_BASE || pc >= FLASH_BASE + FLASH_SIZE) {
        fprintf(out, "  ⚠ PC=0x%08X is OUTSIDE flash! (bug or unmapped jump)\n\n", pc);
    }
}
