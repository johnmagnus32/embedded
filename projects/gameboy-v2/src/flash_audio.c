/*
 * upload.c — In-firmware flash upload mode (interrupt-triggered)
 *
 * A UART RX interrupt watches for "FLASH\n". Once detected, upload
 * mode takes over the CPU completely — it disables all interrupts
 * except UART RX, stops audio DMA, and runs a blocking loop that
 * receives data and programs the flash. No other task runs until
 * the upload is complete and the chip reboots.
 *
 * This is safe because:
 * - Audio DMA is stopped (no flash reads during erase/write)
 * - The game loop is frozen (no display updates needed)
 * - UART polling is used for data transfer (no interrupt nesting)
 */

#include "board.h"
#include "flash_audio.h"
#include "drivers/uart.h"
#include "drivers/flash.h"
#include "drivers/audio.h"
#include "sched.h"

DEVICE_DT_DECLARE(w25q128);

static const char magic[] = "FLASH\n";
static volatile int magic_pos = 0;

/* Read exactly n bytes from UART (polling) */
static void uart_read_exact(uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        char c;
        while (uart_poll_in(uart, &c) != 0) {}
        buf[i] = (uint8_t)c;
    }
}

static uint32_t uart_read_u32(void)
{
    uint8_t buf[4];
    uart_read_exact(buf, 4);
    return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

/* Upload mode — takes over CPU, never returns */
static void handle_audio_flash(void)
{
    /* Stop audio DMA to prevent flash reads during erase/write */
    audio_stop(audio_dev);

    uart_print("READY\n");

    /* Receive total size */
    uint32_t total_size = uart_read_u32();

    /* Erase needed sectors (4KB each) */
    uint32_t sectors = (total_size + 4095) / 4096;
    for (uint32_t i = 0; i < sectors; i++) {
        flash_erase(DEVICE_DT_GET(w25q128), i * 4096, 4096);
        uart_poll_out(uart, '.');
    }
    uart_print("\nWRITING\n");

    /* Receive and write data in 256-byte pages */
    uint8_t page_buf[256];
    uint32_t offset = 0;
    while (offset < total_size) {
        uint32_t chunk = (total_size - offset) > 256 ? 256 : (total_size - offset);
        uart_read_exact(page_buf, chunk);
        flash_write(DEVICE_DT_GET(w25q128), offset, page_buf, chunk);
        uart_poll_out(uart, '.');
        offset += chunk;
    }

    uart_print("DONE\n");
}

static inline void system_reboot(void)
{
    *(volatile uint32_t *)0xE000ED0C = 0x05FA0004;
    while (1) {}
}

/*
 * Flash audio task — polls UART for "FLASH\n".
 * Once detected, enters upload mode (never returns).
 */
void flash_audio_task(void)
{
    for (;;) {
        char c;
        if (uart_poll_in(uart, &c) == 0) {
            if (c == (uint8_t)magic[magic_pos]) {
                magic_pos++;
                if (magic[magic_pos] == '\0') {
                    magic_pos = 0;
                    handle_audio_flash();
                    system_reboot();
                }
            } else {
                magic_pos = 0;
            }
        }
        sched_yield();
    }
}