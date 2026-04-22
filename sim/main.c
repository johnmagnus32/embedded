/*
 * sim/main.c — STM32F411RE Cortex-M4 simulator with GDB-like debugger
 *
 * Usage:
 *   ./sim firmware.elf                    # run, UART to stdout
 *   ./sim firmware.elf --vis              # event-driven visualizer (no breakpoints)
 *   ./sim firmware.elf --debug            # interactive debugger
 *
 * Debugger commands:
 *   break <func>        set breakpoint at function
 *   break <file:line>   set breakpoint at source line
 *   info breakpoints    list breakpoints
 *   delete <n>          delete breakpoint #n
 *   continue (c)        run to next breakpoint
 *   step (s)            step one source line (into calls)
 *   next (n)            step one source line (over calls)
 *   quit (q)            exit
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "cpu.h"
#include "vis.h"
#include "elf_sym.h"

extern void mem_set_uart_fd(int fd);

#define MAX_BP 32
#define PROMPT "\033[33m(dbg) \033[0m"

static void show_state(struct cpu_state *cpu, uint8_t *flash, uint8_t *ram)
{
    uint32_t off;
    const char *fn = sym_lookup(cpu->r[REG_PC], &off);
    int line;
    const char *file = line_lookup(cpu->r[REG_PC], &line);
    char event[80];
    if (fn)
        snprintf(event, sizeof(event), "%s+0x%X", fn, off);
    else
        snprintf(event, sizeof(event), "0x%08X", cpu->r[REG_PC]);
    vis_dump(stderr, cpu, flash, ram, event);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <firmware.elf|.bin> [--debug|--vis]\n", argv[0]);
        return 1;
    }

    int debug_mode = 0, vis_mode = 0;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) debug_mode = 1;
        else if (strcmp(argv[i], "--vis") == 0) vis_mode = 1;
    }

    uint8_t *flash = calloc(1, FLASH_SIZE);
    uint8_t *ram = calloc(1, RAM_SIZE);

    if (elf_load(argv[1], flash, ram) == 0) {
        fprintf(stderr, "Loaded ELF %s\n", argv[1]);
    } else {
        FILE *f = fopen(argv[1], "rb");
        if (!f) { perror("open firmware"); return 1; }
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        fread(flash, 1, size, f);
        fclose(f);
        fprintf(stderr, "Loaded raw binary %s (%ld bytes)\n", argv[1], size);
    }

    struct cpu_state cpu;
    cpu_init(&cpu);
    cpu_reset(&cpu, flash, ram);

    if (!debug_mode && !vis_mode) {
        /* Plain run mode */
        printf("UART output → stdout\n");
        printf("Starting emulation at PC=0x%08X, SP=0x%08X\n", cpu.r[REG_PC], cpu.r[REG_SP]);
        printf("--- UART output ---\n");
        cpu_run(&cpu, flash, ram, 100000000);
        printf("\n--- Emulation ended after %llu cycles ---\n", (unsigned long long)cpu.cycle_count);
        free(flash); free(ram);
        return 0;
    }

    if (vis_mode && !debug_mode) {
        /* Event-driven vis mode (old behavior) */
        extern void cpu_set_vis(FILE *, uint8_t *, uint8_t *);
        cpu_set_vis(stderr, flash, ram);
        vis_dump(stderr, &cpu, flash, ram, "Boot — reset vector");
        fprintf(stderr, "  [Press Enter to start] ");
        getchar();
        cpu_run(&cpu, flash, ram, 100000000);
        free(flash); free(ram);
        return 0;
    }

    /* ── Interactive debugger ── */
    show_state(&cpu, flash, ram);
    fprintf(stderr, "\nStopped at boot. Type 'help' for commands.\n");

    char line_buf[256];
    while (1) {
        fprintf(stderr, PROMPT);
        fflush(stderr);
        if (!fgets(line_buf, sizeof(line_buf), stdin)) break;

        /* Strip newline */
        line_buf[strcspn(line_buf, "\n")] = '\0';
        char *cmd = line_buf;
        while (*cmd == ' ') cmd++;
        if (*cmd == '\0') {
            /* Empty line = repeat last command? For now just show prompt again */
            continue;
        }

        if (strcmp(cmd, "q") == 0 || strcmp(cmd, "quit") == 0) {
            break;

        } else if (strcmp(cmd, "c") == 0 || strcmp(cmd, "continue") == 0) {
            cpu.bp_hit = 0;
            cpu.step_mode = 0;
            cpu_run(&cpu, flash, ram, 100000000);
            if (cpu.bp_hit) {
                show_state(&cpu, flash, ram);
                uint32_t off;
                const char *fn = sym_lookup(cpu.r[REG_PC], &off);
                int ln; line_lookup(cpu.r[REG_PC], &ln);
                fprintf(stderr, "\nBreakpoint hit: %s+0x%X (line %d)\n", fn ? fn : "???", off, ln);
            } else {
                fprintf(stderr, "\nProgram finished (%llu cycles)\n", (unsigned long long)cpu.cycle_count);
            }

        } else if (strcmp(cmd, "s") == 0 || strcmp(cmd, "step") == 0) {
            int cur_line;
            line_lookup(cpu.r[REG_PC], &cur_line);
            cpu.step_mode = 1;
            cpu.step_line = cur_line;
            cpu.bp_hit = 0;
            cpu_run(&cpu, flash, ram, 100000000);
            cpu.step_mode = 0;
            show_state(&cpu, flash, ram);
            fprintf(stderr, "\n");

        } else if (strcmp(cmd, "n") == 0 || strcmp(cmd, "next") == 0) {
            int cur_line;
            line_lookup(cpu.r[REG_PC], &cur_line);
            uint32_t off;
            sym_lookup(cpu.r[REG_PC], &off);
            cpu.step_mode = 2;
            cpu.step_line = cur_line;
            cpu.step_fn_addr = cpu.r[REG_PC] - off;
            cpu.bp_hit = 0;
            cpu_run(&cpu, flash, ram, 100000000);
            cpu.step_mode = 0;
            show_state(&cpu, flash, ram);
            fprintf(stderr, "\n");

        } else if (strncmp(cmd, "break ", 6) == 0 || strncmp(cmd, "b ", 2) == 0) {
            const char *spec = cmd + (cmd[1] == ' ' ? 2 : 6);
            while (*spec == ' ') spec++;
            uint32_t addr = resolve_breakpoint(spec);
            if (addr && cpu.nbp < MAX_BP) {
                cpu.breakpoints[cpu.nbp++] = addr;
                uint32_t off;
                const char *fn = sym_lookup(addr, &off);
                int ln; const char *file = line_lookup(addr, &ln);
                fprintf(stderr, "Breakpoint %d at 0x%08X", cpu.nbp, addr);
                if (fn) fprintf(stderr, " (%s)", fn);
                if (file) fprintf(stderr, " %s:%d", file, ln);
                fprintf(stderr, "\n");
            } else if (!addr) {
                fprintf(stderr, "Cannot resolve '%s'\n", spec);
            } else {
                fprintf(stderr, "Too many breakpoints\n");
            }

        } else if (strcmp(cmd, "info breakpoints") == 0 || strcmp(cmd, "info b") == 0) {
            if (cpu.nbp == 0) {
                fprintf(stderr, "No breakpoints.\n");
            } else {
                for (int i = 0; i < cpu.nbp; i++) {
                    uint32_t off;
                    const char *fn = sym_lookup(cpu.breakpoints[i], &off);
                    int ln; const char *file = line_lookup(cpu.breakpoints[i], &ln);
                    fprintf(stderr, "  %d: 0x%08X", i + 1, cpu.breakpoints[i]);
                    if (fn) fprintf(stderr, "  %s+0x%X", fn, off);
                    if (file) fprintf(stderr, "  %s:%d", file, ln);
                    fprintf(stderr, "\n");
                }
            }

        } else if (strncmp(cmd, "delete ", 7) == 0 || strncmp(cmd, "d ", 2) == 0) {
            int n = atoi(cmd + (cmd[1] == ' ' ? 2 : 7));
            if (n >= 1 && n <= cpu.nbp) {
                for (int i = n - 1; i < cpu.nbp - 1; i++)
                    cpu.breakpoints[i] = cpu.breakpoints[i + 1];
                cpu.nbp--;
                fprintf(stderr, "Deleted breakpoint %d\n", n);
            } else {
                fprintf(stderr, "No breakpoint %d\n", n);
            }

        } else if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0) {
            fprintf(stderr,
                "  break <func|file:line>  set breakpoint\n"
                "  info breakpoints        list breakpoints\n"
                "  delete <n>              remove breakpoint #n\n"
                "  continue (c)            run to next breakpoint\n"
                "  step (s)                step one source line (into)\n"
                "  next (n)                step one source line (over)\n"
                "  quit (q)                exit\n");

        } else {
            fprintf(stderr, "Unknown command: %s (type 'help')\n", cmd);
        }
    }

    free(flash);
    free(ram);
    return 0;
}
