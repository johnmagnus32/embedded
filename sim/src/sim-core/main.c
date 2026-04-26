/*
 * sim-core — ARM Cortex-M4 emulator
 *
 * Reads debugger commands from stdin, writes JSON state to stdout.
 * Launched by sim-web, not run directly.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpu.h"
#include "elf_sym.h"
#include "state.h"
#define MAX_BP 32
#define LOG(fmt, ...) fprintf(stderr, "[sim-core] " fmt "\n", ##__VA_ARGS__)
int g_stopped = 1;
static void emit_state(struct cpu_state *cpu, uint8_t *flash, uint8_t *ram)
{
    state_dump_to(cpu, flash, ram, stdout);
}
int main(int argc, char **argv)
{
    if (argc < 2) {
        LOG("Usage: %s <firmware.elf>", argv[0]);
        return 1;
    }
    uint8_t *flash = calloc(1, FLASH_SIZE);
    uint8_t *ram = calloc(1, RAM_SIZE);
    if (elf_load(argv[1], flash, ram) != 0) {
        LOG("Failed to load ELF: %s", argv[1]);
        return 1;
    }
    LOG("Loaded %s", argv[1]);
    char dir[256]; strncpy(dir, argv[1], 255);
    char *sl = strrchr(dir, '/'); if (sl) *(sl+1)='\0'; else dir[0]='\0';
    state_set_source_dir(dir);
    struct cpu_state cpu;
    cpu_init(&cpu);
    cpu_reset(&cpu, flash, ram);
    setbuf(stdout, NULL);
    /* Auto-run to main() */
    uint32_t main_addr = resolve_breakpoint("main");
    if (main_addr) {
        cpu.breakpoints[0] = main_addr;
        cpu.nbp = 1;
        cpu_run(&cpu, flash, ram, 0);
        cpu.nbp = 0;
    }
    LOG("Stopped at main(), emitting initial state");
    emit_state(&cpu, flash, ram);
    LOG("Waiting for commands on stdin");
    char line_buf[256];
    while (1) {
        if (!fgets(line_buf, sizeof(line_buf), stdin)) break;
        line_buf[strcspn(line_buf, "\n")] = '\0';
        char *cmd = line_buf;
        while (*cmd == ' ') cmd++;
        if (*cmd == '\0') continue;
        LOG("Command: %s", cmd);
        if (strcmp(cmd, "q") == 0 || strcmp(cmd, "quit") == 0) {
            break;
        } else if (strcmp(cmd, "c") == 0 || strcmp(cmd, "continue") == 0) {
            cpu.bp_hit = 0;
            g_stopped = 0;
            /* Run in 500K-cycle chunks, emitting state between for live UART */
            while (!cpu.bp_hit) {
                uint64_t limit = cpu.cycle_count + 500000;
                cpu_run(&cpu, flash, ram, limit);
                if (!cpu.bp_hit)
                    emit_state(&cpu, flash, ram);
            }
            g_stopped = 1;
        } else if (strcmp(cmd, "s") == 0 || strcmp(cmd, "step") == 0) {
            /* Step in: single-step instructions until source line changes.
             * Same approach as sdb: step_instruction in a loop. */
            int orig_line; line_lookup(cpu.r[REG_PC], &orig_line);
            do {
                uint64_t limit = cpu.cycle_count + 1;
                cpu_run(&cpu, flash, ram, limit);
                int cur_line; line_lookup(cpu.r[REG_PC], &cur_line);
                if (cur_line > 0 && cur_line != orig_line) break;
            } while (1);
            cpu.bp_hit = 1;

        } else if (strcmp(cmd, "n") == 0 || strcmp(cmd, "next") == 0) {
            /* Step over: like sdb — if current insn is a BL (call), set temp
             * breakpoint at the return point and run. Otherwise single-step.
             * Repeat until source line changes. */
            int orig_line; line_lookup(cpu.r[REG_PC], &orig_line);
            do {
                uint16_t insn = mem_read16(flash, ram, cpu.r[REG_PC]);
                int is_bl = (insn & 0xF800) == 0xF000;  /* BL first halfword */
                if (is_bl) {
                    /* It's a call — set temp bp at instruction after the BL (4 bytes) */
                    uint32_t ret_point = cpu.r[REG_PC] + 4;
                    int old_nbp = cpu.nbp;
                    cpu.breakpoints[cpu.nbp++] = ret_point;
                    cpu.bp_hit = 0;
                    cpu_run(&cpu, flash, ram, 0);
                    cpu.nbp = old_nbp;
                    cpu.bp_hit = 0;  /* temp bp hit is expected, not a real stop */
                } else {
                    /* Not a call — single step one instruction */
                    uint64_t limit = cpu.cycle_count + 1;
                    cpu_run(&cpu, flash, ram, limit);
                }
                int cur_line; line_lookup(cpu.r[REG_PC], &cur_line);
                if (cur_line > 0 && cur_line != orig_line) break;
            } while (!cpu.bp_hit);
            cpu.bp_hit = 1;  /* remove temp breakpoints */
        } else if (strncmp(cmd, "peek ", 5) == 0) {
            uint32_t addr = (uint32_t)strtoul(cmd + 5, NULL, 0);
            uint32_t val = 0;
            if (addr >= 0x20000000 && addr < 0x20000000 + RAM_SIZE)
                val = *(uint32_t *)(ram + (addr - 0x20000000));
            LOG("peek 0x%08X = %u (0x%08X)", addr, val, val);
        } else if (strncmp(cmd, "break ", 6) == 0 || strncmp(cmd, "b ", 2) == 0) {
            const char *spec = cmd + (cmd[1] == ' ' ? 2 : 6);
            while (*spec == ' ') spec++;
            uint32_t addr = resolve_breakpoint(spec);
            if (addr && cpu.nbp < MAX_BP)
                cpu.breakpoints[cpu.nbp++] = addr;
        } else if (strncmp(cmd, "delete ", 7) == 0 || strncmp(cmd, "d ", 2) == 0) {
            int n = atoi(cmd + (cmd[1] == ' ' ? 2 : 7));
            if (n >= 1 && n <= cpu.nbp) {
                for (int i = n - 1; i < cpu.nbp - 1; i++)
                    cpu.breakpoints[i] = cpu.breakpoints[i + 1];
                cpu.nbp--;
            }
        }
        LOG("Emitting state"); emit_state(&cpu, flash, ram);
    }
    free(flash);
    free(ram);
    return 0;
}
