/*
 * sim-core — ARM Cortex-M4 emulator
 *
 * Loads firmware ELF segments, runs the CPU, serves chardevs.
 * Optionally serves a debug stub on --gdb <port>.
 * --realtime throttles to match the target clock speed.
 *
 * Usage: sim-core --machine <name> --firmware <elf> [--gdb <port>] [--realtime] [--chardev name=port ...]
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

/* ---- Real-time throttle ---- */

static struct timespec rt_start_wall;
static uint64_t rt_start_cycles;
static int rt_init;
static uint64_t rt_target_hz;
static uint64_t rt_check_interval;

static void realtime_throttle(uint64_t cycle_count)
{
    if (!rt_init) {
        clock_gettime(CLOCK_MONOTONIC, &rt_start_wall);
        rt_start_cycles = cycle_count;
        rt_init = 1;
        return;
    }

    uint64_t sim_ticks = cycle_count - rt_start_cycles;
    if (sim_ticks % rt_check_interval != 0) return;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    double wall_elapsed = (now.tv_sec - rt_start_wall.tv_sec)
                        + (now.tv_nsec - rt_start_wall.tv_nsec) / 1e9;
    double sim_elapsed = (double)sim_ticks / (double)rt_target_hz;

    if (sim_elapsed > wall_elapsed) {
        double sleep_sec = sim_elapsed - wall_elapsed;
        if (sleep_sec > 0.0005) {
            struct timespec ts = {
                .tv_sec = (time_t)sleep_sec,
                .tv_nsec = (long)((sleep_sec - (time_t)sleep_sec) * 1e9)
            };
            nanosleep(&ts, NULL);
        }
    }

    /* Log realtime ratio once per second */
    static double last_log = 0;
    if (wall_elapsed - last_log >= 1.0) {
        double ratio = sim_elapsed / wall_elapsed;
        fprintf(stderr, "[realtime] %.2fx\n", ratio);
        last_log = wall_elapsed;
    }
}

/* ---- Main ---- */

int main(int argc, char **argv)
{
    const char *machine_name = NULL;
    const char *elf_path = NULL;
    int gdb_port = 0;
    int realtime = 0;

    static struct chardev_table chardevs;
    chardev_table_init(&chardevs);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--machine") == 0 && i + 1 < argc)
            machine_name = argv[++i];
        else if (strcmp(argv[i], "--firmware") == 0 && i + 1 < argc)
            elf_path = argv[++i];
        else if (strcmp(argv[i], "--gdb") == 0 && i + 1 < argc)
            gdb_port = atoi(argv[++i]);
        else if (strcmp(argv[i], "--chardev") == 0 && i + 1 < argc)
            chardev_add(&chardevs, argv[++i]);
        else if (strcmp(argv[i], "--realtime") == 0)
            realtime = 1;
    }

    if (!machine_name || !elf_path) {
        LOG("Usage: %s --machine <name> --firmware <elf> [--gdb <port>] [--realtime] [--chardev name=port ...]", argv[0]);
        return 1;
    }

    const struct machine_desc *mach = machine_find(machine_name);
    if (!mach) { LOG("Unknown machine: %s", machine_name); return 1; }

    if (chardevs.count > 0)
        chardev_listen_all(&chardevs);

    void *board = calloc(1, mach->board_size);
    mach->init(board, chardevs.count > 0 ? &chardevs : NULL);

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

    if (realtime && mach->get_sysclk) {
        rt_target_hz = mach->get_sysclk(board);
        rt_check_interval = rt_target_hz / 1000;  /* check every ~1ms sim time */
        if (rt_check_interval == 0) rt_check_interval = 1;
        LOG("Real-time throttle: %lu Hz", (unsigned long)rt_target_hz);
    }

    if (gdb_port > 0) {
        struct stub_ctx ctx = {
            .mach = mach, .board = board,
            .cpu = cpu, .bus = bus, .flash = flash, .ram = ram,
            .chardevs = chardevs.count > 0 ? &chardevs : NULL,
        };
        dbg_stub_run(&ctx, gdb_port);
    } else {
        while (1) {
            int r = mach->tick(board);
            if (realtime)
                realtime_throttle(cpu->cycle_count);
            if (r & CPU_SEMIHOST_EXIT) {
                chardev_flush_all(&chardevs);
                chardev_shutdown_all(&chardevs);
                free(*flash); free(*ram); free(board);
                return r & 0xFF;
            }
        }
    }

    free(*flash); free(*ram); free(board);
    return 0;
}
