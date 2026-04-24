/*
 * state.c — Dump emulator state to a JSON file for external visualization
 */

#include <stdio.h>
#include <string.h>
#include "cpu.h"
#include "elf_sym.h"

static char state_path[256];

void state_set_path(const char *path)
{
    strncpy(state_path, path, sizeof(state_path) - 1);
}

void state_dump(struct cpu_state *cpu, uint8_t *flash, uint8_t *ram)
{
    if (!state_path[0]) return;

    FILE *f = fopen(state_path, "w");
    if (!f) return;

    uint32_t pc = cpu->r[REG_PC];
    uint32_t sym_off;
    const char *fn = sym_lookup(pc, &sym_off);
    int line;
    const char *file = line_lookup(pc, &line);

    fprintf(f, "{\n");
    fprintf(f, "  \"pc\": %u,\n", pc);
    fprintf(f, "  \"psp\": %u,\n", cpu->psp);
    fprintf(f, "  \"msp\": %u,\n", cpu->msp);
    fprintf(f, "  \"cycles\": %llu,\n", (unsigned long long)cpu->cycle_count);
    fprintf(f, "  \"in_handler\": %s,\n", cpu->in_handler ? "true" : "false");
    fprintf(f, "  \"function\": \"%s\",\n", fn ? fn : "");
    fprintf(f, "  \"func_offset\": %u,\n", sym_off);
    fprintf(f, "  \"file\": \"%s\",\n", file ? file : "");
    fprintf(f, "  \"line\": %d,\n", line);

    /* Registers */
    fprintf(f, "  \"regs\": [");
    for (int i = 0; i < 16; i++)
        fprintf(f, "%s%u", i ? "," : "", cpu->r[i]);
    fprintf(f, "],\n");

    /* Tasks */
    uint32_t sym_nt = sym_find_by_name("num_tasks");
    uint32_t sym_tasks = sym_find_by_name("tasks");
    uint32_t sym_stacks = sym_find_by_name("task_stacks");
    if (!sym_stacks) sym_stacks = sym_find_by_name("stacks");

    fprintf(f, "  \"tasks\": [\n");
    if (sym_nt && sym_tasks && sym_stacks) {
        int tcb_size, stk_size;
        if (sym_find_by_name("task_stacks")) { tcb_size = 32; stk_size = 512; }
        else { tcb_size = 8; stk_size = 256; }
        uint32_t tasks_off = sym_tasks - RAM_BASE;
        uint32_t stk_base = sym_stacks - RAM_BASE;
        int num_tasks = *(uint32_t *)(ram + (sym_nt - RAM_BASE));
        if (num_tasks > 8) num_tasks = 0;

        for (int t = 0; t < num_tasks; t++) {
            uint32_t sp = *(uint32_t *)(ram + tasks_off + t * tcb_size);
            uint32_t name_ptr = *(uint32_t *)(ram + tasks_off + t * tcb_size + 4);
            char tn[32] = {0};
            if (name_ptr >= FLASH_BASE && name_ptr < FLASH_BASE + FLASH_SIZE) {
                const char *s = (const char *)(flash + (name_ptr - FLASH_BASE));
                for (int j = 0; j < 31 && s[j] >= 0x20 && s[j] < 0x7F; j++) tn[j] = s[j];
            }
            uint32_t stk_top = RAM_BASE + stk_base + (t + 1) * stk_size;
            uint32_t stk_bot = stk_top - stk_size;
            int active = (cpu->psp >= stk_bot && cpu->psp <= stk_top);
            uint32_t dsp = active ? cpu->psp : sp;
            int used = (dsp >= stk_bot && dsp <= stk_top) ? (int)(stk_top - dsp) : 0;

            fprintf(f, "    {\"name\":\"%s\", \"sp\":%u, \"stack_bot\":%u, \"stack_top\":%u, "
                       "\"stack_used\":%d, \"stack_size\":%d, \"active\":%s}%s\n",
                    tn, dsp, stk_bot, stk_top, used, stk_size,
                    active ? "true" : "false",
                    t < num_tasks - 1 ? "," : "");
        }
    }
    fprintf(f, "  ],\n");

    /* Memory layout */
    fprintf(f, "  \"flash_base\": %u,\n", FLASH_BASE);
    fprintf(f, "  \"flash_size\": %u,\n", FLASH_SIZE);
    fprintf(f, "  \"ram_base\": %u,\n", RAM_BASE);
    fprintf(f, "  \"ram_size\": %u\n", RAM_SIZE);
    fprintf(f, "}\n");
    fclose(f);
}
