#include <stdio.h>
#include <stdint.h>
#include "armv7m_cpu.h"
#include "elf_sym.h"
#include "dbg_tasks.h"

int dbg_emit_tasks(struct armv7m_cpu *cpu, uint8_t *flash, uint8_t *ram,
                   char *buf, int bufsize)
{
    int n = 0;
    #define P(...) n += snprintf(buf+n, bufsize-n, __VA_ARGS__)

    uint32_t sym_nt = sym_find_by_name("num_tasks");
    uint32_t sym_tasks = sym_find_by_name("tasks");
    uint32_t sym_stacks = sym_find_by_name("task_stacks");
    if (!sym_stacks) sym_stacks = sym_find_by_name("stacks");

    P("[");
    if (sym_nt && sym_tasks && sym_stacks) {
        uint32_t dwarf_tcb_size; int dwarf_sp_off, dwarf_name_off;
        dwarf_get_tcb_layout(&dwarf_tcb_size, &dwarf_sp_off, &dwarf_name_off);

        int tcb_size = dwarf_tcb_size ? (int)dwarf_tcb_size : 32;
        int sp_off   = dwarf_sp_off >= 0 ? dwarf_sp_off : 0;
        int name_off = dwarf_name_off >= 0 ? dwarf_name_off : 4;
        int stk_size = 512;

        uint32_t tasks_off = sym_tasks - RAM_BASE;
        uint32_t stk_base = sym_stacks - RAM_BASE;
        int num_tasks = *(uint32_t *)(ram + (sym_nt - RAM_BASE));
        if (num_tasks > 8) num_tasks = 0;

        for (int t = 0; t < num_tasks; t++) {
            uint32_t sp = *(uint32_t *)(ram + tasks_off + t * tcb_size + sp_off);
            uint32_t name_ptr = *(uint32_t *)(ram + tasks_off + t * tcb_size + name_off);
            char tn[32] = {0};
            if (name_ptr >= FLASH_BASE && name_ptr < FLASH_BASE + FLASH_SIZE) {
                const char *s = (const char *)(flash + (name_ptr - FLASH_BASE));
                for (int j = 0; j < 31 && s[j] >= 0x20 && s[j] < 0x7F; j++) tn[j] = s[j];
            }
            uint32_t stk_top = RAM_BASE + stk_base + (t + 1) * stk_size;
            uint32_t stk_bot = stk_top - stk_size;
            int active = (cpu->r[REG_SP] >= stk_bot && cpu->r[REG_SP] <= stk_top);
            uint32_t dsp = active ? cpu->r[REG_SP] : sp;
            int used = (dsp >= stk_bot && dsp <= stk_top) ? (int)(stk_top - dsp) : 0;

            if (t) P(",");
            P("{\"name\":\"%s\",\"sp\":%u,\"stack_bot\":%u,\"stack_top\":%u,"
              "\"stack_used\":%d,\"stack_size\":%d,\"active\":%s,\"frames\":[",
              tn, dsp, stk_bot, stk_top, used, stk_size, active ? "true" : "false");

            int nframes = 0;
            if (active) {
                uint32_t sym_off2;
                const char *fn = sym_lookup(cpu->r[REG_PC], &sym_off2);
                if (fn) { P("{\"func\":\"%s\",\"sp\":%u}", fn, dsp); nframes++; }
            }
            for (uint32_t sa = dsp; sa < stk_top && nframes < 8; sa += 4) {
                uint32_t val = *(uint32_t *)(ram + (sa - RAM_BASE));
                if (val >= FLASH_BASE + 1 && val < FLASH_BASE + FLASH_SIZE && (val & 1)) {
                    uint32_t sym_off2;
                    const char *fn = sym_lookup(val & ~1u, &sym_off2);
                    if (fn) { if (nframes) P(","); P("{\"func\":\"%s\",\"sp\":%u}", fn, sa); nframes++; }
                }
            }
            P("],\"stack_data\":[");

            struct stack_var svars[32];
            int nsv = 0;
            if (active) nsv = vars_on_stack(cpu->r[REG_PC], svars, 32);

            int nwords = 0;
            for (uint32_t sa = dsp; sa < stk_top && nwords < 128; sa += 4) {
                uint32_t val = *(uint32_t *)(ram + (sa - RAM_BASE));
                if (nwords) P(",");
                P("{\"addr\":%u,\"val\":%u", sa, val);
                if (val >= FLASH_BASE + 1 && val < FLASH_BASE + FLASH_SIZE && (val & 1)) {
                    uint32_t so2;
                    const char *fn2 = sym_lookup(val & ~1u, &so2);
                    if (fn2) P(",\"sym\":\"%s+0x%x\"", fn2, so2);
                }
                for (int vi = 0; vi < nsv; vi++) {
                    uint32_t var_addr = (uint32_t)((int32_t)dsp + svars[vi].sp_offset);
                    if (var_addr == sa) {
                        P(",\"var\":\"%s\"", svars[vi].name);
                        uint32_t bsz = type_byte_size(svars[vi].type_die);
                        if (bsz > 4) P(",\"vsz\":%u", bsz);
                        break;
                    }
                }
                P("}");
                nwords++;
            }
            P("]}");
        }
    }
    P("]");
    #undef P
    return n;
}
