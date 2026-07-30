/*
 * timer.h — ARMv7 generic (architected) timer, physical timer via CP15 CNTP_*.
 * The wired GIC PPI differs by board (secure phys PPI13->INTID29 on the T113;
 * non-secure phys PPI14->INTID30 on QEMU virt), so the INTID comes from board.h.
 */
#ifndef GV3K_TIMER_H
#define GV3K_TIMER_H

#include <stdint.h>
#include "board.h"

#define TIMER_INTID   BOARD_TIMER_INTID   /* board-specific: 29 (T113) / 30 (virt) */

/* Program the secure physical timer to fire every 1/hz seconds and enable it. */
void timer_init(unsigned hz);

/* Call from the timer IRQ: reload TVAL to schedule the next tick. */
void timer_rearm(void);

/* Detected counter frequency (Hz). */
uint32_t timer_freq(void);

#endif /* GV3K_TIMER_H */
