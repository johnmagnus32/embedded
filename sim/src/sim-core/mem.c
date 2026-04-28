/*
 * mem.c — Memory bus
 *
 * Routes reads/writes to Flash, RAM, or board devices.
 */
#include <stdio.h>
#include <string.h>
#include "cpu.h"
#include "board.h"
#include "trace_dev.h"
#include "spi.h"
#include "ili9341.h"

struct board *g_board = NULL;

static uint32_t nvic_iser[3] = {0};

uint32_t mem_read32(uint8_t *flash, uint8_t *ram, uint32_t addr)
{
    if (addr >= FLASH_BASE && addr < FLASH_BASE + FLASH_SIZE)
        return *(uint32_t *)(flash + (addr - FLASH_BASE));
    if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE)
        return *(uint32_t *)(ram + (addr - RAM_BASE));

    if (g_board) {
        if (trace_dev_handles(&g_board->trace, addr))
            return trace_dev_read(&g_board->trace, addr);
        for (int i = 0; i < g_board->nuarts; i++)
            if (uart_handles(&g_board->uarts[i], addr))
                return uart_read(&g_board->uarts[i], addr);
        for (int i = 0; i < g_board->nspis; i++)
            if (spi_handles(&g_board->spis[i], addr))
                return spi_read(&g_board->spis[i], addr);
        if (systick_handles(addr)) return systick_read(&g_board->systick, addr);
        if (nvic_handles(addr))    return nvic_read(&g_board->nvic, addr);
    }

    if (addr >= 0xE000E100 && addr < 0xE000E10C) return nvic_iser[(addr - 0xE000E100) / 4];
    if (addr == 0xE000ED90) return 0x00000800;
    if (addr >= 0x40023800 && addr < 0x40023900) return 0;
    /* GPIO IDR (offset 0x10) */
    if (g_board && (addr & 0xFFF) == 0x10 && addr >= 0x40020000 && addr < 0x40022000)
        return g_board->gpio_idr;
    if (addr >= 0x40020000 && addr < 0x40021000) return 0;

    /* Warn on first access to unknown address (per 4KB page) */
    static uint32_t warned[16]; static int nwarned;
    uint32_t page = addr & 0xFFFFF000;
    int seen = 0;
    for (int i = 0; i < nwarned; i++) if (warned[i] == page) { seen = 1; break; }
    if (!seen && nwarned < 16) {
        warned[nwarned++] = page;
        fprintf(stderr, "[mem] Unhandled read at 0x%08X (returning 0)\n", addr);
    }
    return 0;
}

void mem_write32(uint8_t *flash, uint8_t *ram, uint32_t addr, uint32_t val)
{
    if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE) {
        *(uint32_t *)(ram + (addr - RAM_BASE)) = val;
        return;
    }

    if (g_board) {
        if (trace_dev_handles(&g_board->trace, addr)) { trace_dev_write(&g_board->trace, addr, val); return; }
        for (int i = 0; i < g_board->nuarts; i++)
            if (uart_handles(&g_board->uarts[i], addr)) { uart_write(&g_board->uarts[i], addr, val); return; }
        for (int i = 0; i < g_board->nspis; i++)
            if (spi_handles(&g_board->spis[i], addr)) { spi_write(&g_board->spis[i], addr, val); return; }
        if (systick_handles(addr)) { systick_write(&g_board->systick, addr, val); return; }
        if (nvic_handles(addr))    { nvic_write(&g_board->nvic, addr, val); return; }
    }

    if (addr >= 0xE000ED90 && addr <= 0xE000EDA0) return;
    if (addr >= 0xE000E100 && addr < 0xE000E10C) { nvic_iser[(addr - 0xE000E100) / 4] |= val; return; }
    if (addr >= 0x40023800 && addr < 0x40023900) return;
    if (addr >= 0x40020000 && addr < 0x40022000) {
        /* GPIO write — check for DC pin change */
        if (g_board && g_board->display && g_board->dc_gpio_pin >= 0) {
            uint32_t gpio_base = addr & 0xFFFFF000;
            int pin = g_board->dc_gpio_pin;
            /* BSRR (offset 0x18): bits [15:0] set, bits [31:16] reset */
            if ((addr & 0xFFF) == 0x18) {
                if (val & (1 << pin)) ili9341_set_dc(g_board->display, 1);
                if (val & (1 << (pin + 16))) ili9341_set_dc(g_board->display, 0);
            }
            /* ODR (offset 0x14) */
            if ((addr & 0xFFF) == 0x14) {
                ili9341_set_dc(g_board->display, (val >> pin) & 1);
            }
        }
        return;
    }
    if (addr >= 0x40013000 && addr < 0x40013100) {
        /* SPI range — handled by spi_handles above if configured */
        return;
    }

    static uint32_t wwarned[16]; static int nwwarned;
    uint32_t wpage = addr & 0xFFFFF000;
    int wseen = 0;
    for (int i = 0; i < nwwarned; i++) if (wwarned[i] == wpage) { wseen = 1; break; }
    if (!wseen && nwwarned < 16) {
        wwarned[nwwarned++] = wpage;
        fprintf(stderr, "[mem] Unhandled write at 0x%08X = 0x%08X (ignored)\n", addr, val);
    }
}

uint16_t mem_read16(uint8_t *flash, uint8_t *ram, uint32_t addr)
{
    if (addr >= FLASH_BASE && addr < FLASH_BASE + FLASH_SIZE)
        return *(uint16_t *)(flash + (addr - FLASH_BASE));
    if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE)
        return *(uint16_t *)(ram + (addr - RAM_BASE));
    return (uint16_t)mem_read32(flash, ram, addr);
}

uint8_t mem_read8(uint8_t *flash, uint8_t *ram, uint32_t addr)
{
    if (addr >= FLASH_BASE && addr < FLASH_BASE + FLASH_SIZE)
        return flash[addr - FLASH_BASE];
    if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE)
        return ram[addr - RAM_BASE];
    return 0;
}

void mem_write16(uint8_t *flash, uint8_t *ram, uint32_t addr, uint16_t val)
{
    if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE) {
        *(uint16_t *)(ram + (addr - RAM_BASE)) = val;
        return;
    }
    mem_write32(flash, ram, addr, val);
}

void mem_write8(uint8_t *flash, uint8_t *ram, uint32_t addr, uint8_t val)
{
    if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE) {
        ram[addr - RAM_BASE] = val;
        return;
    }
    mem_write32(flash, ram, addr, val);
}
