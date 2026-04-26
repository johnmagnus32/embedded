/*
 * mem.c — Memory bus
 *
 * Routes reads/writes to Flash, RAM, or device registers.
 * Device registers are forwarded to the owning module via the board pointer.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpu.h"
#include "board.h"

/* Global board pointer — set by main before any CPU execution */
struct board *g_board = NULL;

/* NVIC ISER (simple — just accept writes) */
static uint32_t nvic_iser[3] = {0};

uint32_t mem_read32(uint8_t *flash, uint8_t *ram, uint32_t addr)
{
    if (addr >= FLASH_BASE && addr < FLASH_BASE + FLASH_SIZE)
        return *(uint32_t *)(flash + (addr - FLASH_BASE));

    if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE)
        return *(uint32_t *)(ram + (addr - RAM_BASE));

    /* USART2 */
    if (addr == USART2_BASE + 0x00) return (1 << 7) | (1 << 6);  /* TXE+TC */
    if (addr == USART2_BASE + 0x04) return 0;

    /* SysTick */
    if (g_board && systick_handles(addr))
        return systick_read(&g_board->systick, addr);

    /* NVIC / SCB */
    if (g_board && nvic_handles(addr))
        return nvic_read(&g_board->nvic, addr);

    /* NVIC ISER */
    if (addr >= 0xE000E100 && addr < 0xE000E10C)
        return nvic_iser[(addr - 0xE000E100) / 4];

    /* MPU_TYPE */
    if (addr == 0xE000ED90) return 0x00000800;

    /* RCC — return nonzero so clock checks pass */
    if (addr >= 0x40023800 && addr < 0x40023900) return 0xFFFFFFFF;

    /* GPIO */
    if (addr >= 0x40020000 && addr < 0x40021000) return 0;

    return 0;
}

void mem_write32(uint8_t *flash, uint8_t *ram, uint32_t addr, uint32_t val)
{
    if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE) {
        *(uint32_t *)(ram + (addr - RAM_BASE)) = val;
        return;
    }

    /* USART2 DR — write character */
    if (addr == USART2_BASE + 0x04) {
        extern void state_uart_putc(char c);
        state_uart_putc((char)(val & 0xFF));
        return;
    }
    if (addr >= USART2_BASE && addr < USART2_BASE + 0x20) return;

    /* SysTick */
    if (g_board && systick_handles(addr)) {
        systick_write(&g_board->systick, addr, val);
        return;
    }

    /* NVIC / SCB */
    if (g_board && nvic_handles(addr)) {
        nvic_write(&g_board->nvic, addr, val);
        return;
    }

    /* MPU — accept silently */
    if (addr >= 0xE000ED90 && addr <= 0xE000EDA0) return;

    /* NVIC ISER */
    if (addr >= 0xE000E100 && addr < 0xE000E10C) {
        nvic_iser[(addr - 0xE000E100) / 4] |= val;
        return;
    }

    /* RCC, GPIO, SPI — ignore */
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
