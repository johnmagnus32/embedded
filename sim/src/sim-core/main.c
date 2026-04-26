/*
 * sim-core — ARM Cortex-M4 emulator + debugger
 *
 * Reads debugger commands from stdin, writes JSON state to stdout.
 * Launched by sim-web, not run directly.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "board.h"
#include "elf_sym.h"
#include "state.h"

#define LOG(fmt, ...) fprintf(stderr, "[sim-core] " fmt "\n", ##__VA_ARGS__)

int g_stopped = 1;

/* Global board pointer for mem.c device routing */
extern struct board *g_board;

static void emit_state(struct board *b)
{
    state_dump_to(&b->cpu, b->flash, b->ram, stdout);
}

static void run_until_bp(struct board *b)
{
    b->bp_hit = 0;
    while (!b->bp_hit)
        board_tick(b);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        LOG("Usage: %s <firmware.elf>", argv[0]);
        return 1;
    }

    struct board board;
    board_init(&board);
    board.flash = calloc(1, FLASH_SIZE);
    board.ram   = calloc(1, RAM_SIZE);
    g_board = &board;

    if (elf_load(argv[1], board.flash, board.ram) != 0) {
        LOG("Failed to load ELF: %s", argv[1]);
        return 1;
    }
    LOG("Loaded %s", argv[1]);

    char dir[256]; strncpy(dir, argv[1], 255);
    char *sl = strrchr(dir, '/'); if (sl) *(sl+1)='\0'; else dir[0]='\0';
    state_set_source_dir(dir);

    cpu_reset(&board.cpu, board.flash, board.ram);

    setbuf(stdout, NULL);

    /* Auto-run to main() */
    uint32_t main_addr = resolve_breakpoint("main");
    if (main_addr) {
        board.breakpoints[0] = main_addr;
        board.nbp = 1;
        run_until_bp(&board);
        board.nbp = 0;
    }
    LOG("Stopped at main(), emitting initial state");
    emit_state(&board);
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
            board.bp_hit = 0;
            g_stopped = 0;
            uint64_t last_emit = board.cpu.cycle_count;
            while (!board.bp_hit) {
                board_tick(&board);
                if (board.cpu.cycle_count - last_emit >= 500000) {
                    emit_state(&board);
                    last_emit = board.cpu.cycle_count;
                }
            }
            g_stopped = 1;

        } else if (strcmp(cmd, "s") == 0 || strcmp(cmd, "step") == 0) {
            int orig_line; line_lookup(board.cpu.r[REG_PC], &orig_line);
            do {
                board_tick(&board);
                int cur_line; line_lookup(board.cpu.r[REG_PC], &cur_line);
                if (cur_line > 0 && cur_line != orig_line) break;
            } while (1);

        } else if (strcmp(cmd, "n") == 0 || strcmp(cmd, "next") == 0) {
            int orig_line; line_lookup(board.cpu.r[REG_PC], &orig_line);
            do {
                uint16_t insn = mem_read16(board.flash, board.ram, board.cpu.r[REG_PC]);
                int is_bl = (insn & 0xF800) == 0xF000;
                if (is_bl) {
                    int old_nbp = board.nbp;
                    board.breakpoints[board.nbp++] = board.cpu.r[REG_PC] + 4;
                    run_until_bp(&board);
                    board.nbp = old_nbp;
                } else {
                    board_tick(&board);
                }
                int cur_line; line_lookup(board.cpu.r[REG_PC], &cur_line);
                if (cur_line > 0 && cur_line != orig_line) break;
            } while (1);

        } else if (strncmp(cmd, "peek ", 5) == 0) {
            uint32_t addr = (uint32_t)strtoul(cmd + 5, NULL, 0);
            uint32_t val = 0;
            if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE)
                val = *(uint32_t *)(board.ram + (addr - RAM_BASE));
            LOG("peek 0x%08X = %u (0x%08X)", addr, val, val);

        } else if (strncmp(cmd, "break ", 6) == 0 || strncmp(cmd, "b ", 2) == 0) {
            const char *spec = cmd + (cmd[1] == ' ' ? 2 : 6);
            while (*spec == ' ') spec++;
            uint32_t addr = resolve_breakpoint(spec);
            if (addr && board.nbp < 32)
                board.breakpoints[board.nbp++] = addr;

        } else if (strncmp(cmd, "delete ", 7) == 0 || strncmp(cmd, "d ", 2) == 0) {
            int n = atoi(cmd + (cmd[1] == ' ' ? 2 : 7));
            if (n >= 1 && n <= board.nbp) {
                for (int i = n - 1; i < board.nbp - 1; i++)
                    board.breakpoints[i] = board.breakpoints[i + 1];
                board.nbp--;
            }
        }

        LOG("Emitting state"); emit_state(&board);
    }

    free(board.flash);
    free(board.ram);
    return 0;
}
