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
#include <signal.h>
#include "cpu.h"
#include "vis.h"
#include "elf_sym.h"

extern void mem_set_uart_fd(int fd);
extern void mem_set_uart_rx_fd(int fd);
extern void mem_set_uart_suppress(int s);

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
    fprintf(stderr, "UART TX → %s  (read:  cat %s)\n", tx_path, tx_path);
    fprintf(stderr, "UART RX ← %s  (write: echo a > %s)\n", rx_path, rx_path);
}

volatile sig_atomic_t dbg_interrupted = 0;
static void sigint_handler(int sig) {
    (void)sig;
    if (dbg_interrupted) {
        /* Second Ctrl+C — force exit */
        signal(SIGINT, SIG_DFL);
        raise(SIGINT);
    }
    dbg_interrupted = 1;
    /* Write a newline so fgets returns */
    write(STDIN_FILENO, "\n", 1);
}

#define MAX_BP 32
#define MAX_CYCLES_RUN   100000000
#define MAX_CYCLES_DEBUG  10000000
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
        fprintf(stderr, "Usage: %s <firmware.elf|.bin> [--debug]\n", argv[0]);
        return 1;
    }

    int debug_mode = 0;
    const char *console_path = NULL;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) debug_mode = 1;
        else if (strcmp(argv[i], "--console") == 0 && i + 1 < argc)
            console_path = argv[++i];
    }

    uint8_t *flash = calloc(1, FLASH_SIZE);
    uint8_t *ram = calloc(1, RAM_SIZE);

    if (elf_load(argv[1], flash, ram) == 0) {
        fprintf(stderr, "Loaded ELF %s\n", argv[1]);
        vis_set_source_dir(argv[1]);
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

    if (!debug_mode) {
        /* Plain run mode */
        if (console_path) {
            setup_console(console_path);
        } else {
            printf("UART output → stdout\n");
        }
        printf("Starting emulation at PC=0x%08X, SP=0x%08X\n", cpu.r[REG_PC], cpu.r[REG_SP]);
        printf("--- UART output ---\n");
        cpu_run(&cpu, flash, ram, MAX_CYCLES_RUN);
        printf("\n--- Emulation ended after %llu cycles ---\n", (unsigned long long)cpu.cycle_count);
        free(flash); free(ram);
        return 0;
    }

    /* Debug mode — UART to fifo */
    const char *fifo = console_path ? console_path : "/tmp/sim_uart";
    setup_console(fifo);

    /* ── Interactive debugger ── */
    signal(SIGINT, sigint_handler);  /* Ctrl+C interrupts continue, doesn't exit */
    /* Auto-run to main() so we start somewhere useful */
    uint32_t main_addr = resolve_breakpoint("main");
    if (main_addr) {
        cpu.breakpoints[0] = main_addr;
        cpu.nbp = 1;
        cpu_run(&cpu, flash, ram, MAX_CYCLES_DEBUG);
        cpu.nbp = 0;  /* remove temporary breakpoint */
        vis_dbg_log("Stopped at main(). Type 'help'.");
    } else {
        vis_dbg_log("Stopped at boot. Type 'help'.");
    }
    show_state(&cpu, flash, ram);

    char line_buf[256];
    while (1) {
        dbg_interrupted = 0;
        fprintf(stderr, PROMPT);
        fflush(stderr);
        if (!fgets(line_buf, sizeof(line_buf), stdin)) break;
        if (dbg_interrupted) break;  /* Ctrl+C at prompt = quit */

        line_buf[strcspn(line_buf, "\n")] = '\0';
        char *cmd = line_buf;
        while (*cmd == ' ') cmd++;
        if (*cmd == '\0') continue;

        if (strcmp(cmd, "q") == 0 || strcmp(cmd, "quit") == 0) {
            break;

        } else if (strcmp(cmd, "c") == 0 || strcmp(cmd, "continue") == 0) {
            cpu.bp_hit = 0;
            cpu.step_mode = 0;
            /* Run in chunks until breakpoint or Ctrl+C */
            extern volatile sig_atomic_t dbg_interrupted;
            dbg_interrupted = 0;
            while (!cpu.bp_hit && !dbg_interrupted && cpu.running) {
                cpu_run(&cpu, flash, ram, cpu.cycle_count + MAX_CYCLES_DEBUG);
                if (!cpu.bp_hit && !dbg_interrupted)
                    continue;  /* keep going */
            }
            if (cpu.bp_hit) {
                uint32_t off;
                const char *fn = sym_lookup(cpu.r[REG_PC], &off);
                int ln; line_lookup(cpu.r[REG_PC], &ln);
                fprintf(stderr, "\nBreakpoint: %s+0x%X line %d\n", fn ? fn : "???", off, ln);
            } else if (dbg_interrupted) {
                fprintf(stderr, "\nInterrupted.\n");
            }
            show_state(&cpu, flash, ram);

        } else if (strcmp(cmd, "s") == 0 || strcmp(cmd, "step") == 0) {
            int cur_line;
            line_lookup(cpu.r[REG_PC], &cur_line);
            cpu.step_mode = 1;
            cpu.step_line = cur_line;
            cpu.bp_hit = 0;
            cpu_run(&cpu, flash, ram, MAX_CYCLES_DEBUG);
            cpu.step_mode = 0;
            show_state(&cpu, flash, ram);

        } else if (strcmp(cmd, "n") == 0 || strcmp(cmd, "next") == 0) {
            int cur_line;
            line_lookup(cpu.r[REG_PC], &cur_line);
            uint32_t off;
            sym_lookup(cpu.r[REG_PC], &off);
            cpu.step_mode = 2;
            cpu.step_line = cur_line;
            cpu.step_max_line = cur_line;
            cpu.step_fn_addr = cpu.r[REG_PC] - off;
            cpu.bp_hit = 0;
            cpu_run(&cpu, flash, ram, MAX_CYCLES_DEBUG);
            cpu.step_mode = 0;
            show_state(&cpu, flash, ram);

        } else if (strncmp(cmd, "break ", 6) == 0 || strncmp(cmd, "b ", 2) == 0) {
            const char *spec = cmd + (cmd[1] == ' ' ? 2 : 6);
            while (*spec == ' ') spec++;
            uint32_t addr = resolve_breakpoint(spec);
            if (addr && cpu.nbp < MAX_BP) {
                cpu.breakpoints[cpu.nbp++] = addr;
                uint32_t off;
                const char *fn = sym_lookup(addr, &off);
                int ln; const char *file = line_lookup(addr, &ln);
                vis_dbg_log("BP %d: 0x%08X %s %s:%d", cpu.nbp, addr,
                    fn ? fn : "", file ? file : "", ln);
            } else if (!addr) {
                vis_dbg_log("Cannot resolve '%s'", spec);
            }
            show_state(&cpu, flash, ram);

        } else if (strcmp(cmd, "info breakpoints") == 0 || strcmp(cmd, "info b") == 0) {
            if (cpu.nbp == 0) {
                vis_dbg_log("No breakpoints.");
            } else {
                for (int i = 0; i < cpu.nbp; i++) {
                    uint32_t off;
                    const char *fn = sym_lookup(cpu.breakpoints[i], &off);
                    int ln; line_lookup(cpu.breakpoints[i], &ln);
                    vis_dbg_log(" %d: %s+0x%X line %d", i+1, fn ? fn : "???", off, ln);
                }
            }
            show_state(&cpu, flash, ram);

        } else if (strncmp(cmd, "delete ", 7) == 0 || strncmp(cmd, "d ", 2) == 0) {
            int n = atoi(cmd + (cmd[1] == ' ' ? 2 : 7));
            if (n >= 1 && n <= cpu.nbp) {
                for (int i = n - 1; i < cpu.nbp - 1; i++)
                    cpu.breakpoints[i] = cpu.breakpoints[i + 1];
                cpu.nbp--;
                vis_dbg_log("Deleted breakpoint %d", n);
            } else {
                vis_dbg_log("No breakpoint %d", n);
            }
            show_state(&cpu, flash, ram);

        } else if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0) {
            vis_dbg_log("break <func|file:line>");
            vis_dbg_log("info breakpoints");
            vis_dbg_log("delete <n>");
            vis_dbg_log("continue (c)  step (s)  next (n)");
            vis_dbg_log("quit (q)");
            show_state(&cpu, flash, ram);

        } else {
            vis_dbg_log("Unknown: %s", cmd);
            show_state(&cpu, flash, ram);
        }
    }

    free(flash);
    free(ram);
    return 0;
}
