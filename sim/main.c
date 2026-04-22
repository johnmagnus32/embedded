/*
 * sim/main.c — STM32F411RE simulator
 *
 * Loads a .bin firmware file into emulated flash,
 * runs the Cortex-M4 emulator, UART output goes to stdout
 * (or a FIFO for monitoring in another terminal).
 *
 * Usage:
 *   ./sim ../sample-stm32f411re/build/hello.bin
 *   ./sim ../sample-stm32f411re/build/hello.bin --fifo /tmp/uart
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "cpu.h"

extern void mem_set_uart_fd(int fd);

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <firmware.bin> [--fifo <path>]\n", argv[0]);
        return 1;
    }

    /* Load firmware binary */
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror("open firmware"); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    printf("Loading %s (%ld bytes)\n", argv[1], size);

    /* Allocate flash and RAM */
    uint8_t *flash = calloc(1, FLASH_SIZE);
    uint8_t *ram = calloc(1, RAM_SIZE);

    if (size > FLASH_SIZE) {
        fprintf(stderr, "Firmware too large (%ld > %d)\n", size, FLASH_SIZE);
        return 1;
    }
    fread(flash, 1, size, f);
    fclose(f);

    /* Optional FIFO for UART output */
    if (argc >= 4 && strcmp(argv[2], "--fifo") == 0) {
        const char *fifo_path = argv[3];
        mkfifo(fifo_path, 0666);
        printf("UART output → %s (open with: cat %s)\n", fifo_path, fifo_path);
        printf("Waiting for reader...\n");
        int fd = open(fifo_path, O_WRONLY);
        if (fd < 0) { perror("open fifo"); return 1; }
        mem_set_uart_fd(fd);
        printf("Connected.\n");
    } else {
        printf("UART output → stdout\n");
    }

    /* Init and run CPU */
    struct cpu_state cpu;
    cpu_init(&cpu);
    cpu_reset(&cpu, flash, ram);

    printf("Starting emulation at PC=0x%08X, SP=0x%08X\n", cpu.r[REG_PC], cpu.r[REG_SP]);
    printf("--- UART output ---\n");

    cpu_run(&cpu, flash, ram, 100000000);  /* run up to 100M instructions */

    printf("\n--- Emulation ended after %llu cycles ---\n", (unsigned long long)cpu.cycle_count);
    if (!cpu.running)
        printf("CPU halted at PC=0x%08X\n", cpu.r[REG_PC]);

    free(flash);
    free(ram);
    return 0;
}
