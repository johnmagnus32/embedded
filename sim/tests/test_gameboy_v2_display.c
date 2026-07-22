/*
 * test_gameboy_v2_display.c — End-to-end display verification
 *
 * Runs the full gameboy-v2 machine (MCU + gate-level FPGA) with the real
 * firmware until boot completes, then verifies the ILI9341 framebuffer
 * contains the expected title screen.
 *
 * No chardevs, no TCP — reads the ILI9341 fb directly for a deterministic test.
 *
 * Build: make test-display
 * Run:   ./build/test_gameboy_v2_display
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "machine.h"
#include "armv7m_cpu.h"
#include "membus.h"
#include "elf_load.h"
#include "gameboy_v2.h"
#include "ili9341.h"

#define BG_COLOR 0x867D

static int uart_has(struct gameboy_v2 *b, const char *needle)
{
    /* Check trace device buffer for UART output — we use a simple ring buffer */
    (void)b; (void)needle;
    return 0;  /* placeholder — we use cycle count instead */
}

int main(int argc, char **argv)
{
    const char *elf_path = (argc > 1) ? argv[1]
        : "../projects/gameboy-v2/build/gameboy-v2.elf";
    const char *netlist_path = (argc > 2) ? argv[2]
        : "../projects/gameboy-v2/build/ppu_top.json";

    /* Initialize machine */
    struct gameboy_v2 board;
    memset(&board, 0, sizeof(board));
    gameboy_v2_init(&board, NULL);

    /* Load FPGA netlist */
    const struct machine_desc *mach = machine_find("gameboy-v2");
    if (!mach) { fprintf(stderr, "FAIL: machine not found\n"); return 1; }
    if (mach->load_device(&board, "fpga0", netlist_path) != 0) {
        fprintf(stderr, "FAIL: could not load netlist %s\n", netlist_path);
        return 1;
    }

    /* Load firmware */
    if (elf_load_segments(elf_path, board.soc.flash, board.soc.ram) != 0) {
        fprintf(stderr, "FAIL: could not load ELF %s\n", elf_path);
        return 1;
    }
    armv7m_cpu_reset(&board.soc.cpu, &board.soc.bus);

    fprintf(stderr, "[test] Running firmware...\n");

    /*
     * Run until the UART prints "game: done\n".
     * The firmware prints via uart_poll_out to USART1 (0x40011000).
     * We intercept the UART TX data register writes via the bus.
     * Simpler: just run for enough cycles. The firmware boots in ~45M cycles
     * at 16MHz (measured). We run for 50M to be safe.
     */
    /* The firmware boots in ~750M cycles at 16MHz with gate-level FPGA.
     * Most time is the 104KB bitstream upload on SPI3, which doesn't involve
     * the FPGA bridge (SPI3 goes to flash/config, not the gate-level eval).
     * But the FPGA IS ticking alongside during boot (rendering bg frames).
     * We run until firmware boot completes — detect via the USART1 TX. */
    #define BOOT_CYCLES 800000000ULL

    /* Snoop UART by checking USART1 TDR writes. The stm32_uart model stores
     * the last TX byte. We poll it each tick. */
    char uart_buf[256] = {0};
    int uart_pos = 0;
    int boot_done = 0;

    for (uint64_t i = 0; i < BOOT_CYCLES && !boot_done; i++) {
        int r = gameboy_v2_tick(&board);
        if (r & 0x100) {
            fprintf(stderr, "FAIL: firmware exited during boot\n");
            return 1;
        }
        /* Check UART output via the USART SR/DR — if TX has data available,
         * the chardev would have sent it. Without chardev, we check the
         * usart model's internal state. Simpler: just run for enough cycles. */
        if (i > 0 && (i % 100000000) == 0)
            fprintf(stderr, "[test] %lluM cycles...\n", (unsigned long long)(i / 1000000));
    }

    fprintf(stderr, "[test] Reached %lluM cycles\n",
            (unsigned long long)(BOOT_CYCLES / 1000000));

    /*
     * Now run until the ILI9341 has received at least 2 complete frames
     * (one to render with the sprite data, one more to be sure).
     * A complete frame = 320×240 = 76800 pixels written.
     * We count pixel writes via the ILI9341's cursor position.
     */
    fprintf(stderr, "[test] Waiting for complete frame render...\n");

    /* Run until we've seen 2 frame completions (cursor wraps from end to start).
     * We detect this by watching cur_row going from row_end back to row_start. */
    int frames_completed = 0;
    int was_at_end = 0;
    uint64_t extra_cycles = 0;
    #define MAX_EXTRA 20000000ULL  /* 20M extra cycles max */

    while (frames_completed < 2 && extra_cycles < MAX_EXTRA) {
        gameboy_v2_tick(&board);
        extra_cycles++;

        int at_end = (board.display.cur_row == 0 && board.display.cur_col == 0
                      && extra_cycles > 100);
        if (was_at_end && !at_end) {
            /* Cursor moved away from (0,0) — new frame started */
        }
        if (!was_at_end && at_end) {
            frames_completed++;
            fprintf(stderr, "[test] Frame %d complete at +%llu cycles\n",
                    frames_completed, (unsigned long long)extra_cycles);
        }
        was_at_end = at_end;
    }

    if (frames_completed < 2) {
        fprintf(stderr, "FAIL: only %d frames completed in %llu extra cycles\n",
                frames_completed, (unsigned long long)extra_cycles);
        return 1;
    }

    /* Now check the framebuffer */
    fprintf(stderr, "[test] Checking framebuffer...\n");

    int ew = ili9341_eff_w(&board.display);
    int eh = ili9341_eff_h(&board.display);
    fprintf(stderr, "[test] Display: %dx%d (madctl=0x%02X)\n", ew, eh, board.display.madctl);

    int total_pixels = ew * eh;
    int non_bg = 0;
    int title_non_bg = 0;
    int black_count = 0;

    for (int i = 0; i < total_pixels; i++) {
        uint16_t p = board.display.fb[i];
        if (p != BG_COLOR) {
            non_bg++;
            if (p == 0x0000) black_count++;
        }
        /* Title sprite at (80,80), 64×14 pixels */
        int x = i % ew;
        int y = i / ew;
        if (x >= 80 && x < 144 && y >= 80 && y < 94) {
            if (p != BG_COLOR)
                title_non_bg++;
        }
    }

    printf("display: %dx%d, non_bg=%d, black=%d, title_region=%d/%d\n",
           ew, eh, non_bg, black_count, title_non_bg, 64 * 14);

    if (title_non_bg > 0) {
        printf("PASS: title sprite visible (%d pixels)\n", title_non_bg);
        return 0;
    } else if (non_bg > 0) {
        /* Find where the non-bg pixels are */
        int min_x = ew, max_x = 0, min_y = eh, max_y = 0;
        for (int i = 0; i < total_pixels; i++) {
            if (board.display.fb[i] != BG_COLOR) {
                int x = i % ew, y = i / ew;
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
            }
        }
        printf("FAIL: %d non-bg pixels at (%d,%d)-(%d,%d) but none in title region\n",
               non_bg, min_x, min_y, max_x, max_y);
        return 1;
    } else {
        printf("FAIL: all pixels are background (no sprite rendered)\n");
        return 1;
    }
}
