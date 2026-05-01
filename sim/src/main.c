/*
 * sim-core — ARM Cortex-M4 emulator
 *
 * Loads firmware ELF segments, runs the CPU, serves chardevs.
 * Optionally serves a debug stub on --debug <port>.
 *
 * Usage: sim-core --machine <name> --firmware <elf> [--debug <port>] [--chardev name=port ...]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "machine.h"
#include "armv7m_cpu.h"
#include "membus.h"
#include "chardev.h"
#include "elf_load.h"
#include "dbg_stub.h"

#define LOG(fmt, ...) fprintf(stderr, "[sim-core] " fmt "\n", ##__VA_ARGS__)

int main(int argc, char **argv)
{
    const char *machine_name = NULL;
    const char *elf_path = NULL;
    int debug_port = 0;  /* 0 = no debug stub */
    uint64_t bench_cycles = 0;
    int no_chardev = 0;
    int headless = 0;

    struct chardev_table chardevs;
    chardev_table_init(&chardevs);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--machine") == 0 && i + 1 < argc) {
            machine_name = argv[++i];
        } else if (strcmp(argv[i], "--firmware") == 0 && i + 1 < argc) {
            elf_path = argv[++i];
        } else if (strcmp(argv[i], "--debug") == 0 && i + 1 < argc) {
            debug_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--chardev") == 0 && i + 1 < argc) {
            chardev_add(&chardevs, argv[++i]);
        } else if (strcmp(argv[i], "--bench") == 0 && i + 1 < argc) {
            bench_cycles = strtoull(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--no-chardev") == 0) {
            no_chardev = 1;
        } else if (strcmp(argv[i], "--headless") == 0) {
            headless = 1;
            no_chardev = 1;
        }
    }

    if (!machine_name || !elf_path) {
        LOG("Usage: %s --machine <name> --firmware <elf> [--debug <port>] [--chardev name=port ...]", argv[0]);
        return 1;
    }

    const struct machine_desc *mach = machine_find(machine_name);
    if (!mach) {
        LOG("Unknown machine: %s", machine_name);
        return 1;
    }

    if (!no_chardev)
        chardev_listen_all(&chardevs);

    void *board = calloc(1, mach->board_size);
    mach->init(board, no_chardev ? NULL : &chardevs);

    struct armv7m_cpu *cpu = mach->get_cpu(board);
    struct membus *bus = mach->get_bus(board);
    uint8_t **flash = mach->get_flash(board);
    uint8_t **ram = mach->get_ram(board);

    if (elf_load_segments(elf_path, *flash, *ram) != 0) {
        LOG("Failed to load ELF: %s", elf_path);
        return 1;
    }
    LOG("Loaded %s", elf_path);

    armv7m_cpu_reset(cpu, bus);

    if (headless) {
        while (1) {
            int r = mach->tick(board);
            if (r & CPU_SEMIHOST_EXIT) {
                free(*flash); free(*ram); free(board);
                return r & 0xFF;
            }
        }
    }

    if (bench_cycles > 0) {
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (uint64_t i = 0; i < bench_cycles; i++) {
            int r = mach->tick(board);
            if (r & CPU_SEMIHOST_EXIT) {
                free(*flash); free(*ram); free(board);
                return r & 0xFF;
            }
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double secs = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
        LOG("Bench: %llu ticks in %.3fs = %.1f MIPS",
            (unsigned long long)bench_cycles, secs, bench_cycles / secs / 1e6);
        free(*flash); free(*ram); free(board);
        return 0;
    }

    if (debug_port > 0) {
        struct stub_ctx ctx = {
            .mach = mach, .board = board,
            .cpu = cpu, .bus = bus, .flash = flash, .ram = ram,
            .chardevs = no_chardev ? NULL : &chardevs,
        };
        dbg_stub_run(&ctx, debug_port);
    } else {
        /* No debug port — just run forever (chardevs serve the UI) */
        while (1) {
            int r = mach->tick(board);
            if (r & CPU_SEMIHOST_EXIT) {
                if (!no_chardev) {
                    chardev_flush_all(&chardevs);
                    chardev_shutdown_all(&chardevs);
                }
                free(*flash); free(*ram); free(board);
                return r & 0xFF;
            }
        }
    }

    free(*flash);
    free(*ram);
    free(board);
    return 0;
}
