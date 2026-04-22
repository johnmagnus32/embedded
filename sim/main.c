/*
 * sim/main.c — STM32F411RE Cortex-M4 simulator
 *
 * Usage:
 *   ./sim firmware.elf                          # run, UART to stdout
 *   ./sim firmware.elf --vis                    # event-driven visualizer
 *   ./sim firmware.elf --vis --break main       # break at function
 *   ./sim firmware.elf --vis --break task_a --break systick_handler
 *   ./sim firmware.elf --vis --break test_rtos.c:110
 *   ./sim firmware.elf --vis --break 0x08000100
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

#define MAX_BREAKPOINTS 32

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <firmware.elf|.bin> [--vis] [--break <spec>...]\n", argv[0]);
        fprintf(stderr, "  --break <func>          break at function entry\n");
        fprintf(stderr, "  --break <file:line>      break at source line\n");
        fprintf(stderr, "  --break 0x<addr>        break at address\n");
        return 1;
    }

    /* Parse args */
    int vis_enabled = 0;
    const char *break_specs[MAX_BREAKPOINTS];
    int nbreak_specs = 0;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--vis") == 0) vis_enabled = 1;
        else if (strcmp(argv[i], "--break") == 0 && i + 1 < argc)
            break_specs[nbreak_specs++] = argv[++i];
    }

    /* Allocate flash and RAM */
    uint8_t *flash = calloc(1, FLASH_SIZE);
    uint8_t *ram = calloc(1, RAM_SIZE);

    /* Try loading as ELF first, fall back to raw binary */
    if (elf_load(argv[1], flash, ram) == 0) {
        printf("Loaded ELF %s\n", argv[1]);
    } else {
        FILE *f = fopen(argv[1], "rb");
        if (!f) { perror("open firmware"); return 1; }
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (size > FLASH_SIZE) {
            fprintf(stderr, "Firmware too large (%ld > %d)\n", size, FLASH_SIZE);
            return 1;
        }
        fread(flash, 1, size, f);
        fclose(f);
        printf("Loaded raw binary %s (%ld bytes)\n", argv[1], size);
    }

    /* Resolve breakpoints */
    uint32_t breakpoints[MAX_BREAKPOINTS];
    int nbp = 0;
    for (int i = 0; i < nbreak_specs; i++) {
        uint32_t addr = resolve_breakpoint(break_specs[i]);
        if (addr) {
            breakpoints[nbp++] = addr;
            uint32_t off;
            const char *fn = sym_lookup(addr, &off);
            int line;
            const char *file = line_lookup(addr, &line);
            fprintf(stderr, "Breakpoint %d: %s → 0x%08X", nbp, break_specs[i], addr);
            if (fn) fprintf(stderr, " (%s+0x%X)", fn, off);
            if (file) fprintf(stderr, " %s:%d", file, line);
            fprintf(stderr, "\n");
        } else {
            fprintf(stderr, "Warning: cannot resolve breakpoint '%s'\n", break_specs[i]);
        }
    }

    /* Optional FIFO for UART output */
    for (int i = 2; i < argc - 1; i++) {
        if (strcmp(argv[i], "--fifo") == 0) {
            const char *fifo_path = argv[i + 1];
            mkfifo(fifo_path, 0666);
            int fd = open(fifo_path, O_WRONLY);
            if (fd >= 0) mem_set_uart_fd(fd);
            break;
        }
    }
    if (!vis_enabled) printf("UART output → stdout\n");

    /* Init and run CPU */
    struct cpu_state cpu;
    cpu_init(&cpu);
    cpu_reset(&cpu, flash, ram);

    if (!vis_enabled) {
        printf("Starting emulation at PC=0x%08X, SP=0x%08X\n", cpu.r[REG_PC], cpu.r[REG_SP]);
        printf("--- UART output ---\n");
        cpu_run(&cpu, flash, ram, 100000000);
        printf("\n--- Emulation ended after %llu cycles ---\n", (unsigned long long)cpu.cycle_count);
    } else if (nbp == 0) {
        /* No breakpoints — use old event-driven mode */
        extern void cpu_set_vis(FILE *, uint8_t *, uint8_t *);
        cpu_set_vis(stderr, flash, ram);
        vis_dump(stderr, &cpu, flash, ram, "Boot — reset vector");
        fprintf(stderr, "  [Press Enter to start] ");
        getchar();
        cpu_run(&cpu, flash, ram, 100000000);
    } else {
        /* Breakpoint mode — set breakpoints in cpu state, run until hit */
        for (int i = 0; i < nbp && i < 32; i++)
            cpu.breakpoints[i] = breakpoints[i];
        cpu.nbp = nbp;

        vis_dump(stderr, &cpu, flash, ram, "Boot — reset vector");
        fprintf(stderr, "\n  [Enter] run to breakpoint ");
        getchar();

        while (cpu.running && cpu.cycle_count < 100000000) {
            cpu.bp_hit = 0;
            cpu_run(&cpu, flash, ram, 100000000);
            if (!cpu.bp_hit) break;

            uint32_t off;
            const char *fn = sym_lookup(cpu.r[REG_PC], &off);
            char event[80];
            snprintf(event, sizeof(event), "Break: %s+0x%X", fn ? fn : "???", off);
            vis_dump(stderr, &cpu, flash, ram, event);
            fprintf(stderr, "\n  [Enter] continue ");
            getchar();
        }
    }

    free(flash);
    free(ram);
    return 0;
}
