/*
 * mem.c — Memory bus
 *
 * Routes reads/writes to Flash, RAM, or board devices.
 */
#include <stdio.h>
#include <string.h>
#include "cpu.h"
#include "board.h"

struct board *g_board = NULL;

static uint32_t nvic_iser[3] = {0};

uint32_t mem_read32(uint8_t *flash, uint8_t *ram, uint32_t addr)
{
    if (addr >= FLASH_BASE && addr < FLASH_BASE + FLASH_SIZE)
        return *(uint32_t *)(flash + (addr - FLASH_BASE));
    if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE)
        return *(uint32_t *)(ram + (addr - RAM_BASE));

    if (g_board) {
        if (uart_handles(addr)) {
            return uart_read(&g_board->uart, addr);
        }
        if (systick_handles(addr)) return systick_read(&g_board->systick, addr);
        if (nvic_handles(addr))    return nvic_read(&g_board->nvic, addr);
    }

    if (addr >= 0xE000E100 && addr < 0xE000E10C) return nvic_iser[(addr - 0xE000E100) / 4];
    if (addr == 0xE000ED90) return 0x00000800;
    if (addr >= 0x40023800 && addr < 0x40023900) return 0xFFFFFFFF;
    if (addr >= 0x40020000 && addr < 0x40021000) return 0;
    return 0;
}

void mem_write32(uint8_t *flash, uint8_t *ram, uint32_t addr, uint32_t val)
{
    if (addr >= 0x40000000 && addr < 0x50000000) {
    }
    if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE) {
        *(uint32_t *)(ram + (addr - RAM_BASE)) = val;
        return;
    }

    if (g_board) {
        if (uart_handles(addr))    { uart_write(&g_board->uart, addr, val); return; }
        if (systick_handles(addr)) { systick_write(&g_board->systick, addr, val); return; }
        if (nvic_handles(addr))    { nvic_write(&g_board->nvic, addr, val); return; }
    }

    if (addr >= 0xE000ED90 && addr <= 0xE000EDA0) return;
    if (addr >= 0xE000E100 && addr < 0xE000E10C) { nvic_iser[(addr - 0xE000E100) / 4] |= val; return; }
    if (addr >= 0x40023800 && addr < 0x40023900) return;
    if (addr >= 0x40020000 && addr < 0x40022000) return;
    if (addr >= 0x40013000 && addr < 0x40013100) return;
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
