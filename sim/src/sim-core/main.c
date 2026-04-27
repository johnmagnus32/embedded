/*
 * sim-core — ARM Cortex-M4 emulator + debugger
 *
 * CPU/debugger state → stdout (JSON per line)
 * Device state → separate files in /tmp/sim-state/
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "board.h"
#include "elf_sym.h"
#include "state.h"

#define LOG(fmt, ...) fprintf(stderr, "[sim-core] " fmt "\n", ##__VA_ARGS__)
#define STATE_DIR "/tmp/sim-state"

extern struct board *g_board;

static uint32_t breakpoints[32];
static int nbp = 0;

static void emit_state(struct board *b)
{
    state_dump_to(&b->cpu, b->flash, b->ram, stdout);
}

static int check_breakpoint(struct board *b)
{
    uint32_t pc = b->cpu.r[REG_PC];
    for (int i = 0; i < nbp; i++)
        if (pc == breakpoints[i]) return 1;
    return 0;
}

static void run_until_bp(struct board *b)
{
    do { board_tick(b); } while (!check_breakpoint(b));
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        LOG("Usage: %s <firmware.elf>", argv[0]);
        return 1;
    }

    /* State directory for device files */
    mkdir(STATE_DIR, 0755);

    struct board board;
    board_init(&board);
    board.flash = calloc(1, FLASH_SIZE);
    board.ram   = calloc(1, RAM_SIZE);
    g_board = &board;

    /* Configure device state output */
    uart_set_state_dir(STATE_DIR);

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
        breakpoints[0] = main_addr;
        nbp = 1;
        run_until_bp(&board);
        nbp = 0;
    }
    LOG("Stopped at main()");
    emit_state(&board);

    char line_buf[256];
    while (fgets(line_buf, sizeof(line_buf), stdin)) {
        line_buf[strcspn(line_buf, "\n")] = '\0';
        char *cmd = line_buf;
        while (*cmd == ' ') cmd++;
        if (*cmd == '\0') continue;

        LOG("Command: %s", cmd);

        if (strcmp(cmd, "c") == 0 || strcmp(cmd, "continue") == 0) {
            do { board_tick(&board); } while (!check_breakpoint(&board));

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
                    int old_nbp = nbp;
                    breakpoints[nbp++] = board.cpu.r[REG_PC] + 4;
                    run_until_bp(&board);
                    nbp = old_nbp;
                } else {
                    board_tick(&board);
                }
                int cur_line; line_lookup(board.cpu.r[REG_PC], &cur_line);
                if (cur_line > 0 && cur_line != orig_line) break;
            } while (1);

        } else if (strncmp(cmd, "b ", 2) == 0 || strncmp(cmd, "break ", 6) == 0) {
            const char *spec = cmd + (cmd[1] == ' ' ? 2 : 6);
            while (*spec == ' ') spec++;
            uint32_t addr = resolve_breakpoint(spec);
            if (addr && nbp < 32) breakpoints[nbp++] = addr;

        } else if (strncmp(cmd, "d ", 2) == 0 || strncmp(cmd, "delete ", 7) == 0) {
            int n = atoi(cmd + (cmd[1] == ' ' ? 2 : 7));
            if (n >= 1 && n <= nbp) {
                for (int i = n - 1; i < nbp - 1; i++)
                    breakpoints[i] = breakpoints[i + 1];
                nbp--;
            }
        }

        emit_state(&board);
    }

    free(board.flash);
    free(board.ram);
    return 0;
}
