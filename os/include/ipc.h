/*
 * ipc.h — Inter-processor communication API
 *
 * Generic mailbox interface for AMP systems.
 * Like Linux's include/linux/mailbox_client.h
 * and Zephyr's include/zephyr/ipc/ipc_service.h
 */

#ifndef IPC_H
#define IPC_H

#include "device.h"
#include <stdint.h>

struct ipc_driver_api {
    int  (*send)(const struct device *dev, const void *data, uint32_t len);
    int  (*recv)(const struct device *dev, void *buf, uint32_t buf_size);
};

static inline int ipc_send(const struct device *dev, const void *data, uint32_t len)
{
    const struct ipc_driver_api *api = dev->api;
    return api->send(dev, data, len);
}

static inline int ipc_recv(const struct device *dev, void *buf, uint32_t buf_size)
{
    const struct ipc_driver_api *api = dev->api;
    return api->recv(dev, buf, buf_size);
}

#endif
