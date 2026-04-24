/*
 * sim-core — ARM Cortex-M4 emulator
 *
 * Reads debugger commands from stdin, writes JSON state to stdout.
 * Launched by sim-web, not run directly.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "cpu.h"
#include "elf_sym.h"
#include "state.h"


volatile sig_atomic_t dbg_interrupted = 0;
static void sigint_handler(int sig) { (void)sig; dbg_interrupted = 1; }

#define MAX_BP 32

static void emit_state(struct cpu_state *cpu, uint8_t *flash, uint8_t *ram)
{
    state_dump_to(cpu, flash, ram, stdout);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "[sim-core] Usage: %s <firmware.elf>\n", argv[0]);
        return 1;
    }

    uint8_t *flash = calloc(1, FLASH_SIZE);
    uint8_t *ram = calloc(1, RAM_SIZE);

    if (elf_load(argv[1], flash, ram) != 0) {
        fprintf(stderr, "[sim-core] Failed to load ELF: %s\n", argv[1]);
        return 1;
    }
    fprintf(stderr, "[sim-core] Loaded %s\n", argv[1]);

    char dir[256]; strncpy(dir, argv[1], 255);
    char *sl = strrchr(dir, '/'); if (sl) *(sl+1)='\0'; else dir[0]='\0';
    state_set_source_dir(dir);

    struct cpu_state cpu;
    cpu_init(&cpu);
    cpu_reset(&cpu, flash, ram);

    setbuf(stdout, NULL);
    signal(SIGINT, sigint_handler);

    /* Auto-run to main() */
    uint32_t main_addr = resolve_breakpoint("main");
    if (main_addr) {
        cpu.breakpoints[0] = main_addr;
        cpu.nbp = 1;
        cpu_run(&cpu, flash, ram, 0);
        cpu.nbp = 0;
    }
    emit_state(&cpu, flash, ram);

    char line_buf[256];
    while (1) {
        if (!fgets(line_buf, sizeof(line_buf), stdin)) break;
        if (dbg_interrupted) break;

        line_buf[strcspn(line_buf, "\n")] = '\0';
        char *cmd = line_buf;
        while (*cmd == ' ') cmd++;
        if (*cmd == '\0') continue;

        if (strcmp(cmd, "q") == 0 || strcmp(cmd, "quit") == 0) {
            break;

        } else if (strcmp(cmd, "c") == 0 || strcmp(cmd, "continue") == 0) {
            cpu.bp_hit = 0;
            cpu.step_mode = 0;
            dbg_interrupted = 0;
            while (!cpu.bp_hit && !dbg_interrupted && cpu.running)
                cpu_run(&cpu, flash, ram, 0);

        } else if (strcmp(cmd, "s") == 0 || strcmp(cmd, "step") == 0) {
            int cur_line; line_lookup(cpu.r[REG_PC], &cur_line);
            cpu.step_mode = 1;
            cpu.step_line = cur_line;
            cpu.bp_hit = 0;
            cpu_run(&cpu, flash, ram, 0);
            cpu.step_mode = 0;

        } else if (strcmp(cmd, "n") == 0 || strcmp(cmd, "next") == 0) {
            int cur_line; line_lookup(cpu.r[REG_PC], &cur_line);
            uint32_t off; sym_lookup(cpu.r[REG_PC], &off);
            cpu.step_mode = 2;
            cpu.step_line = cur_line;
            cpu.step_max_line = cur_line;
            cpu.step_fn_addr = cpu.r[REG_PC] - off;
            cpu.bp_hit = 0;
            cpu_run(&cpu, flash, ram, 0);
            cpu.step_mode = 0;

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

        emit_state(&cpu, flash, ram);
    }

    free(flash);
    free(ram);
    return 0;
}
