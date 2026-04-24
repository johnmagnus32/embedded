/*
 * mem.c — Memory access with peripheral emulation
 *
 * Reads/writes to flash and RAM go to host memory.
 * Reads/writes to peripheral addresses are intercepted
 * and emulate the hardware behavior.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cpu.h"

#define SYSTICK_BASE 0xE000E010

/* SysTick state */
static uint32_t systick_csr = 0;
static uint32_t systick_rvr = 0;
static uint32_t systick_cvr = 0;

/* SCB state */
static uint32_t scb_icsr = 0;
static uint32_t scb_shpr3 = 0;
static uint32_t scb_vtor = 0;
static uint32_t scb_shcsr = 0;

/* NVIC state */
static uint32_t nvic_iser[3] = {0};

uint32_t mem_read32(uint8_t *flash, uint8_t *ram, uint32_t addr)
{
    /* Flash */
    if (addr >= FLASH_BASE && addr < FLASH_BASE + FLASH_SIZE) {
        uint32_t off = addr - FLASH_BASE;
        return *(uint32_t *)(flash + off);
    }

    /* RAM */
    if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE) {
        uint32_t off = addr - RAM_BASE;
        return *(uint32_t *)(ram + off);
    }

    /* USART2 */
    if (addr == USART2_BASE + 0x00) { /* SR */
        return (1 << 7) | (1 << 6);  /* TXE + TC always ready */
    }
    if (addr == USART2_BASE + 0x04) { /* DR */
        return 0;
    }

    /* SysTick */
    if (addr == SYSTICK_BASE + 0x00) return systick_csr;
    if (addr == SYSTICK_BASE + 0x04) return systick_rvr;
    if (addr == SYSTICK_BASE + 0x08) return systick_cvr;

    /* SCB */
    if (addr == 0xE000ED04) return scb_icsr;
    if (addr == 0xE000ED08) return scb_vtor;
    if (addr == 0xE000ED20) return scb_shpr3;
    if (addr == 0xE000ED24) return scb_shcsr;
    if (addr == 0xE000ED90) return 0x00000800;  /* MPU_TYPE: 8 regions */

    /* NVIC ISER */
    if (addr >= 0xE000E100 && addr < 0xE000E10C)
        return nvic_iser[(addr - 0xE000E100) / 4];

    /* RCC — return nonzero so clock checks pass */
    if (addr >= 0x40023800 && addr < 0x40023900)
        return 0xFFFFFFFF;

    /* GPIO — return 0 (no buttons pressed) */
    if (addr >= 0x40020000 && addr < 0x40021000)
        return 0;

    return 0;
}


void mem_write32(uint8_t *flash, uint8_t *ram, uint32_t addr, uint32_t val)
{
    /* RAM */
    if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE) {
        uint32_t off = addr - RAM_BASE;
        *(uint32_t *)(ram + off) = val;
        return;
    }

    /* USART2 DR — write character */
    if (addr == USART2_BASE + 0x04) {
        char c = (char)(val & 0xFF);
        extern void state_uart_putc(char c);
        state_uart_putc(c);
        return;
    }

    /* USART2 other regs — ignore */
    if (addr >= USART2_BASE && addr < USART2_BASE + 0x20)
        return;

    /* SysTick */
    if (addr == SYSTICK_BASE + 0x00) { systick_csr = val; return; }
    if (addr == SYSTICK_BASE + 0x04) { systick_rvr = val; return; }
    if (addr == SYSTICK_BASE + 0x08) { systick_cvr = val; return; }

    /* SCB */
    if (addr == 0xE000ED04) { scb_icsr = val; return; }
    if (addr == 0xE000ED08) { scb_vtor = val; return; }
    if (addr == 0xE000ED20) { scb_shpr3 = val; return; }
    if (addr == 0xE000ED24) { scb_shcsr = val; return; }

    /* MPU — accept writes silently */
    if (addr >= 0xE000ED90 && addr <= 0xE000EDA0)
        return;

    /* NVIC */
    if (addr >= 0xE000E100 && addr < 0xE000E10C) {
        nvic_iser[(addr - 0xE000E100) / 4] |= val;
        return;
    }

    /* RCC — ignore */
    if (addr >= 0x40023800 && addr < 0x40023900)
        return;

    /* GPIO — ignore */
    if (addr >= 0x40020000 && addr < 0x40022000)
        return;

    /* SPI — ignore */
    if (addr >= 0x40013000 && addr < 0x40013100)
        return;
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

int systick_enabled(void) { return systick_csr & 1; }
int systick_irq_enabled(void) { return systick_csr & 2; }
uint32_t systick_reload(void) { return systick_rvr; }
int pendsv_pending(void) { return scb_icsr & (1 << 28); }
void clear_pendsv(void) { scb_icsr &= ~(1 << 28); }
int svc_pending(void) { return scb_icsr & (1 << 15); }
void clear_svc(void) { scb_icsr &= ~(1 << 15); }
