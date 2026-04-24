/*
 * sim-core — ARM Cortex-M4 emulator
 *
 * Plain:    ./sim-core firmware.elf              (UART to stdout)
 * Headless: ./sim-core firmware.elf --headless   (JSON to stdout, commands from stdin)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include "cpu.h"
#include "elf_sym.h"
#include "state.h"

extern void mem_set_uart_fd(int fd);
extern void mem_set_uart_rx_fd(int fd);
extern void mem_set_uart_suppress(int s);

volatile sig_atomic_t dbg_interrupted = 0;
static void sigint_handler(int sig) { (void)sig; dbg_interrupted = 1; }

#define MAX_BP 32
#define MAX_CYCLES_RUN   100000000
#define MAX_CYCLES_DEBUG  10000000

static void setup_console(const char *path)
{
    char tx_path[256], rx_path[256];
    snprintf(tx_path, sizeof(tx_path), "%s", path);
    snprintf(rx_path, sizeof(rx_path), "%s.in", path);
    mkfifo(tx_path, 0666);
    mkfifo(rx_path, 0666);
    signal(SIGPIPE, SIG_IGN);
    int tx_fd = open(tx_path, O_RDWR | O_NONBLOCK);
    int rx_fd = open(rx_path, O_RDWR | O_NONBLOCK);
    if (tx_fd >= 0) mem_set_uart_fd(tx_fd);
    if (rx_fd >= 0) mem_set_uart_rx_fd(rx_fd);
    mem_set_uart_suppress(1);
}

static void show_state(struct cpu_state *cpu, uint8_t *flash, uint8_t *ram)
{
    state_dump_to(cpu, flash, ram, stdout);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <firmware.elf|.bin> [--headless] [--console <path>]\n", argv[0]);
        return 1;
    }

    int headless = 0;
    const char *console_path = NULL;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--headless") == 0) headless = 1;
        else if (strcmp(argv[i], "--console") == 0 && i + 1 < argc)
            console_path = argv[++i];
    }

    uint8_t *flash = calloc(1, FLASH_SIZE);
    uint8_t *ram = calloc(1, RAM_SIZE);

    if (elf_load(argv[1], flash, ram) == 0) {
        fprintf(stderr, "Loaded ELF %s\n", argv[1]);
        /* Set source search dir for state.c */
        char dir[256]; strncpy(dir, argv[1], 255);
        char *sl = strrchr(dir, '/'); if (sl) *(sl+1)='\0'; else dir[0]='\0';
        state_set_source_dir(dir);
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

    if (console_path) setup_console(console_path);

    struct cpu_state cpu;
    cpu_init(&cpu);
    cpu_reset(&cpu, flash, ram);

    /* ── Plain run mode ── */
    if (!headless) {
        if (!console_path) printf("UART output → stdout\n");
        printf("Starting emulation at PC=0x%08X, SP=0x%08X\n", cpu.r[REG_PC], cpu.r[REG_SP]);
        printf("--- UART output ---\n");
        cpu_run(&cpu, flash, ram, MAX_CYCLES_RUN);
        printf("\n--- Emulation ended after %llu cycles ---\n", (unsigned long long)cpu.cycle_count);
        free(flash); free(ram);
        return 0;
    }

    /* ── Headless debugger mode ── */
    setbuf(stdout, NULL);
    signal(SIGINT, sigint_handler);
    mem_set_uart_suppress(1);

    /* Auto-run to main() */
    uint32_t main_addr = resolve_breakpoint("main");
    if (main_addr) {
        cpu.breakpoints[0] = main_addr;
        cpu.nbp = 1;
        cpu_run(&cpu, flash, ram, MAX_CYCLES_DEBUG);
        cpu.nbp = 0;
    }
    show_state(&cpu, flash, ram);

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
                cpu_run(&cpu, flash, ram, cpu.cycle_count + MAX_CYCLES_DEBUG);
            show_state(&cpu, flash, ram);

        } else if (strcmp(cmd, "s") == 0 || strcmp(cmd, "step") == 0) {
            int cur_line; line_lookup(cpu.r[REG_PC], &cur_line);
            cpu.step_mode = 1;
            cpu.step_line = cur_line;
            cpu.bp_hit = 0;
            cpu_run(&cpu, flash, ram, cpu.cycle_count + MAX_CYCLES_DEBUG);
            cpu.step_mode = 0;
            show_state(&cpu, flash, ram);

        } else if (strcmp(cmd, "n") == 0 || strcmp(cmd, "next") == 0) {
            int cur_line; line_lookup(cpu.r[REG_PC], &cur_line);
            uint32_t off; sym_lookup(cpu.r[REG_PC], &off);
            cpu.step_mode = 2;
            cpu.step_line = cur_line;
            cpu.step_max_line = cur_line;
            cpu.step_fn_addr = cpu.r[REG_PC] - off;
            cpu.bp_hit = 0;
            cpu_run(&cpu, flash, ram, cpu.cycle_count + MAX_CYCLES_DEBUG);
            cpu.step_mode = 0;
            show_state(&cpu, flash, ram);

        } else if (strncmp(cmd, "break ", 6) == 0 || strncmp(cmd, "b ", 2) == 0) {
            const char *spec = cmd + (cmd[1] == ' ' ? 2 : 6);
            while (*spec == ' ') spec++;
            uint32_t addr = resolve_breakpoint(spec);
            if (addr && cpu.nbp < MAX_BP)
                cpu.breakpoints[cpu.nbp++] = addr;
            show_state(&cpu, flash, ram);

        } else if (strcmp(cmd, "info breakpoints") == 0 || strcmp(cmd, "info b") == 0) {
            show_state(&cpu, flash, ram);

        } else if (strncmp(cmd, "delete ", 7) == 0 || strncmp(cmd, "d ", 2) == 0) {
            int n = atoi(cmd + (cmd[1] == ' ' ? 2 : 7));
            if (n >= 1 && n <= cpu.nbp) {
                for (int i = n - 1; i < cpu.nbp - 1; i++)
                    cpu.breakpoints[i] = cpu.breakpoints[i + 1];
                cpu.nbp--;
            }
            show_state(&cpu, flash, ram);

        } else {
            show_state(&cpu, flash, ram);
        }
    }

    free(flash);
    free(ram);
    return 0;
}
