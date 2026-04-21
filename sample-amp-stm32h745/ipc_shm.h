/*
 * ipc_shm.h — Shared memory IPC for AMP (Asymmetric Multi-Processing)
 *
 * Both cores include this header. It defines a mailbox in shared RAM
 * that both cores can read/write. This is the simplest form of
 * inter-processor communication.
 *
 * On STM32H745:
 *   - M7 core has its own 512KB RAM at 0x24000000
 *   - M4 core has its own 128KB RAM at 0x30000000
 *   - Shared RAM (SRAM4) at 0x38000000 (64KB) — both can access
 *
 * Maps to Zephyr's subsys/ipc/ and OpenAMP.
 */

#ifndef IPC_SHM_H
#define IPC_SHM_H

#include <stdint.h>

/* Shared memory region — must be at the same address for both cores */
#define IPC_SHM_BASE  0x38000000

/* Message channels (M7→M4 and M4→M7) */
struct ipc_channel {
    volatile uint32_t flag;     /* 0 = empty, 1 = message ready */
    volatile uint32_t len;
    volatile char data[56];     /* 56 bytes payload */
};

/* Shared memory layout */
struct ipc_shm {
    struct ipc_channel m7_to_m4;  /* M7 writes, M4 reads */
    struct ipc_channel m4_to_m7;  /* M4 writes, M7 reads */
};

#define IPC ((volatile struct ipc_shm *)IPC_SHM_BASE)

/* Send a message to the other core */
static inline void ipc_send(volatile struct ipc_channel *ch,
                            const char *msg, uint32_t len)
{
    if (len > 56) len = 56;
    for (uint32_t i = 0; i < len; i++)
        ch->data[i] = msg[i];
    ch->len = len;
    ch->flag = 1;  /* signal: message ready */
}

/* Check if a message is available */
static inline int ipc_recv(volatile struct ipc_channel *ch,
                           char *buf, uint32_t buf_size)
{
    if (!ch->flag) return 0;  /* no message */

    uint32_t len = ch->len;
    if (len > buf_size) len = buf_size;
    for (uint32_t i = 0; i < len; i++)
        buf[i] = ch->data[i];

    ch->flag = 0;  /* acknowledge: message consumed */
    return (int)len;
}

#endif
