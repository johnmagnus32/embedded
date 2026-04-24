/*
 * sim-core — ARM Cortex-M4 emulator
 *
 * Reads debugger commands from stdin, writes JSON state to stdout.
 * Launched by sim-web, not run directly.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "cpu.h"
#include "elf_sym.h"
#include "state.h"

extern void mem_set_uart_rx_fd(int fd);



#define MAX_BP 32
#define LOG(fmt, ...) fprintf(stderr, "[sim-core] " fmt "\n", ##__VA_ARGS__)

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

    /* Pipe for UART RX: browser → sim-core → firmware */
    int uart_pipe[2];
    pipe(uart_pipe);
    fcntl(uart_pipe[0], F_SETFL, O_NONBLOCK);
    mem_set_uart_rx_fd(uart_pipe[0]);

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
            cpu.step_mode = 0;
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

        } else if (strncmp(cmd, "u ", 2) == 0) {
            /* UART input: write bytes to the RX pipe */
            const char *text = cmd + 2;
            write(uart_pipe[1], text, strlen(text));
            write(uart_pipe[1], "\r", 1);
            LOG("UART RX: %zu bytes", strlen(text) + 1);
        }

        LOG("Emitting state"); emit_state(&cpu, flash, ram);
    }

    free(flash);
    free(ram);
    return 0;
}
