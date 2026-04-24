/*
 * state.c — Dump emulator state to JSON (stdout or file)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpu.h"
#include "elf_sym.h"
#include "state.h"

static char state_path[256];
static char src_search_dir[256];

/* UART TX capture ring buffer */
#define UART_CAP_SIZE 8192
static char uart_cap[UART_CAP_SIZE];
static int uart_cap_head = 0;
static int uart_cap_count = 0;

void state_set_path(const char *path) { strncpy(state_path, path, sizeof(state_path) - 1); }
void state_set_source_dir(const char *dir) { strncpy(src_search_dir, dir, sizeof(src_search_dir) - 1); }

void state_uart_putc(char c)
{
    if (c == '\r') return;
    uart_cap[uart_cap_head] = c;
    uart_cap_head = (uart_cap_head + 1) % UART_CAP_SIZE;
    if (uart_cap_count < UART_CAP_SIZE) uart_cap_count++;
}

/* Load source file and return lines array */
static char *src_lines[4096];
static int src_nlines;
static char src_file[256];

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

static void json_escape(FILE *f, const char *s)
{
    for (; *s; s++) {
        if (*s == '"') fprintf(f, "\\\"");
        else if (*s == '\\') fprintf(f, "\\\\");
        else if (*s == '\t') fprintf(f, "    ");
        else if ((unsigned char)*s >= 0x20) fputc(*s, f);
    }
}

void state_dump(struct cpu_state *cpu, uint8_t *flash, uint8_t *ram)
{
    state_dump_to(cpu, flash, ram, NULL);
}

void state_dump_to(struct cpu_state *cpu, uint8_t *flash, uint8_t *ram, FILE *out)
{
    FILE *f = out;
    if (!f && state_path[0]) f = fopen(state_path, "w");
    if (!f) return;

    uint32_t pc = cpu->r[REG_PC];
    uint32_t sym_off;
    const char *fn = sym_lookup(pc, &sym_off);
    int cur_line;
    const char *file = line_lookup(pc, &cur_line);

    fprintf(f, "{");
    extern int g_stopped;
    fprintf(f, "\"stopped\":%s,", g_stopped ? "true" : "false");
    fprintf(f, "\"pc\":%u,", pc);
    fprintf(f, "\"psp\":%u,", cpu->psp);
    fprintf(f, "\"msp\":%u,", cpu->msp);
    fprintf(f, "\"cycles\":%llu,", (unsigned long long)cpu->cycle_count);
    fprintf(f, "\"in_handler\":%s,", cpu->in_handler ? "true" : "false");
    fprintf(f, "\"function\":\"%s\",", fn ? fn : "");
    fprintf(f, "\"func_offset\":%u,", sym_off);
    fprintf(f, "\"file\":\"%s\",", file ? file : "");
    fprintf(f, "\"line\":%d,", cur_line);

    /* Registers */
    fprintf(f, "\"regs\":[");
    for (int i = 0; i < 16; i++)
        fprintf(f, "%s%u", i ? "," : "", cpu->r[i]);
    fprintf(f, "],");

    /* Source code context */
    if (file) load_source(file);
    fprintf(f, "\"source\":{\"file\":\"%s\",\"current_line\":%d,\"lines\":[", file ? file : "", cur_line);
    if (src_nlines > 0 && cur_line > 0) {
        for (int l = 1; l <= src_nlines; l++) {
            if (l > 1) fprintf(f, ",");
            fprintf(f, "{\"num\":%d,\"text\":\"", l);
            json_escape(f, src_lines[l - 1]);
            fprintf(f, "\"}");
        }
    }
    fprintf(f, "]},");

    /* UART output (ring buffer — output in order) */
    fprintf(f, "\"uart\":\"");
    int start = (uart_cap_count < UART_CAP_SIZE) ? 0 : uart_cap_head;
    for (int j = 0; j < uart_cap_count; j++) {
        char c = uart_cap[(start + j) % UART_CAP_SIZE];
        if (c == '"') fprintf(f, "\\\"");
        else if (c == '\\') fprintf(f, "\\\\");
        else if (c == '\n') fprintf(f, "\\n");
        else if ((unsigned char)c >= 0x20) fputc(c, f);
    }
    fprintf(f, "\",");

    /* Tasks */
    uint32_t sym_nt = sym_find_by_name("num_tasks");
    uint32_t sym_tasks = sym_find_by_name("tasks");
    uint32_t sym_stacks = sym_find_by_name("task_stacks");
    if (!sym_stacks) sym_stacks = sym_find_by_name("stacks");

    fprintf(f, "\"tasks\":[");
    if (sym_nt && sym_tasks && sym_stacks) {
        uint32_t dwarf_tcb_size; int dwarf_sp_off, dwarf_name_off;
        dwarf_get_tcb_layout(&dwarf_tcb_size, &dwarf_sp_off, &dwarf_name_off);

        int tcb_size = dwarf_tcb_size ? (int)dwarf_tcb_size : 32;
        int sp_off   = dwarf_sp_off >= 0 ? dwarf_sp_off : 0;
        int name_off = dwarf_name_off >= 0 ? dwarf_name_off : 4;
        int stk_size = 512; /* TODO: derive from DWARF TASK_STACK_SIZE */

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
            int active = (cpu->psp >= stk_bot && cpu->psp <= stk_top);
            uint32_t dsp = active ? cpu->psp : sp;
            int used = (dsp >= stk_bot && dsp <= stk_top) ? (int)(stk_top - dsp) : 0;

            if (t) fprintf(f, ",");
            fprintf(f, "{\"name\":\"%s\",\"sp\":%u,\"stack_bot\":%u,\"stack_top\":%u,"
                       "\"stack_used\":%d,\"stack_size\":%d,\"active\":%s}",
                    tn, dsp, stk_bot, stk_top, used, stk_size, active ? "true" : "false");
        }
    }
    fprintf(f, "],");

    /* ELF sections */
    const struct elf_section *secs;
    int nsecs = elf_get_sections(&secs);
    fprintf(f, "\"sections\":[");
    for (int i = 0; i < nsecs; i++) {
        if (i) fprintf(f, ",");
        fprintf(f, "{\"name\":\"%s\",\"addr\":%u,\"size\":%u}", secs[i].name, secs[i].addr, secs[i].size);
    }
    fprintf(f, "],");

    fprintf(f, "\"ram_base\":%u,\"ram_size\":%u}\n", RAM_BASE, RAM_SIZE);

    if (!out && state_path[0]) fclose(f);
    if (out) fflush(out);
}
