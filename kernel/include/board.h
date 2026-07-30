/*
 * board.h — the board-abstraction seam.
 *
 * The kernel's ARM-architected code (MMU, vectors, generic timer via CP15, the
 * GICv2 programming model) is identical across boards; only a few peripheral
 * ADDRESSES and the UART hardware differ. This header selects them at build time
 * so the same kernel sources build for either target:
 *
 *   make BOARD=t113   (default) — Allwinner T113-S3 (real hardware, via bootloader)
 *   make BOARD=virt              — QEMU `-M virt -cpu cortex-a7` (for actually
 *                                   running/debugging the kernel logic)
 *
 * The Makefile passes -DBOARD_VIRT for the virt build; T113 is the default.
 * uart.c dispatches to the right UART driver on the same flag.
 */
#ifndef GV3K_BOARD_H
#define GV3K_BOARD_H

#if defined(BOARD_VIRT)
/* ---- QEMU 'virt' (GICv2, PL011 UART) — addresses from the generated DTB ---- */
#define BOARD_NAME        "qemu-virt"
#define GICD_BASE         0x08000000u   /* GIC distributor  */
#define GICC_BASE         0x08010000u   /* GIC CPU interface */
/* virt (no EL3/secure) routes the NON-secure physical timer: PPI 14 -> INTID 30.
 * (The T113 firmware leaves us in secure state -> secure phys PPI 13 -> INTID 29.)
 * Both use the same CP15 CNTP_* registers; only the wired PPI differs. */
#define BOARD_TIMER_INTID 30
/* UART: PL011 @ 0x09000000 (see board_pl011.c) — no clock/pinmux setup needed. */
#define BOARD_UART_PL011  1
#define BOARD_UART_BASE   0x09000000u

#else
/* ---- Allwinner T113-S3 (default) — GIC-400, 16550 UART on PE2/PE3 ----------- */
#define BOARD_NAME        "t113-s3"
#define GICD_BASE         0x03021000u
#define GICC_BASE         0x03022000u
#define BOARD_TIMER_INTID 29            /* secure physical timer PPI 13 -> INTID 29 */
#define BOARD_UART_16550  1
#define BOARD_UART_BASE   0x02500000u

#endif

#endif /* GV3K_BOARD_H */
