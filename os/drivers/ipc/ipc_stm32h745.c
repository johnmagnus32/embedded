/*
 * ipc_stm32h745.c — STM32H745 shared memory mailbox driver
 *
 * Chip-specific: knows the shared SRAM4 layout for M7↔M4 communication.
 * Base address comes from device tree.
 *
 * Linux equivalent: drivers/mailbox/stm32-ipcc.c
 * Zephyr equivalent: drivers/ipm/ipm_stm32_ipcc.c
 *
 * The real STM32H745 has a hardware IPCC peripheral with interrupts.
 * We use a simpler polled shared-memory approach.
 */

#include <stdint.h>
#include "config.h"

#ifdef CONFIG_IPC_STM32H745

#include "devicetree.h"
#include "device.h"
#include "ipc.h"

#define REG(base, off) (*(volatile uint32_t *)((base) + (off)))

/*
 * Shared memory layout (at the DT-provided base address):
 *
 * Offset 0x00: M7→M4 channel
 *   [0x00] flag (0=empty, 1=ready)
 *   [0x04] length
 *   [0x08] data[56]
 *
 * Offset 0x40: M4→M7 channel
 *   [0x40] flag
 *   [0x44] length
 *   [0x48] data[56]
 */

#define CH_FLAG  0x00
#define CH_LEN   0x04
#define CH_DATA  0x08
#define CH_SIZE  0x40  /* 64 bytes per channel */

struct ipc_stm32h745_config {
    uint32_t base;
    uint8_t  tx_channel;  /* 0 = M7→M4 (M7 sends), 1 = M4→M7 (M4 sends) */
    uint8_t  rx_channel;  /* opposite of tx */
};

static int ipc_stm32h745_send(const struct device *dev, const void *data, uint32_t len)
{
    const struct ipc_stm32h745_config *cfg = dev->config;
    uint32_t ch_base = cfg->base + cfg->tx_channel * CH_SIZE;

    if (len > 56) len = 56;

    /* Write data */
    const uint8_t *src = data;
    for (uint32_t i = 0; i < len; i++)
        ((volatile uint8_t *)(ch_base + CH_DATA))[i] = src[i];

    REG(ch_base, CH_LEN) = len;
    REG(ch_base, CH_FLAG) = 1;  /* signal: message ready */

    return (int)len;
}

static int ipc_stm32h745_recv(const struct device *dev, void *buf, uint32_t buf_size)
{
    const struct ipc_stm32h745_config *cfg = dev->config;
    uint32_t ch_base = cfg->base + cfg->rx_channel * CH_SIZE;

    if (!REG(ch_base, CH_FLAG))
        return 0;  /* no message */

    uint32_t len = REG(ch_base, CH_LEN);
    if (len > buf_size) len = buf_size;

    uint8_t *dst = buf;
    for (uint32_t i = 0; i < len; i++)
        dst[i] = ((volatile uint8_t *)(ch_base + CH_DATA))[i];

    REG(ch_base, CH_FLAG) = 0;  /* acknowledge */

    return (int)len;
}

static const struct ipc_driver_api ipc_stm32h745_api = {
    .send = ipc_stm32h745_send,
    .recv = ipc_stm32h745_recv,
};

/*
 * Two instances: one for M7 (tx=ch0, rx=ch1) and one for M4 (tx=ch1, rx=ch0).
 * Which one gets created depends on the DT — M7's DTS has tx-channel=0,
 * M4's DTS has tx-channel=1.
 */
#define _IPC_LABEL(n) DT_INST_SHARED_MEMORY_IPC_##n##_LABEL
#define _IPC_PROP(n, p) DT_INST_SHARED_MEMORY_IPC_##n##_PROP_##p

#define STM32H745_IPC_DEFINE(n)                                     \
    static const struct ipc_stm32h745_config ipc_cfg_##n = {        \
        .base       = DT_INST_SHARED_MEMORY_IPC_##n##_REG_ADDR,    \
        .tx_channel = _IPC_PROP(n, TX_CHANNEL),                     \
        .rx_channel = _IPC_PROP(n, RX_CHANNEL),                     \
    };                                                              \
    DEVICE_DT_DEFINE(_IPC_LABEL(n),                                 \
                     NULL, NULL, &ipc_cfg_##n, &ipc_stm32h745_api);

DT_INST_FOREACH_STATUS_OKAY(SHARED_MEMORY_IPC, STM32H745_IPC_DEFINE)

#endif /* CONFIG_IPC_STM32H745 */
