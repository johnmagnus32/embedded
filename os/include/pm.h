/*
 * pm.h — Per-device runtime power management
 *
 * Two approaches:
 *
 * 1. Manual (like Zephyr):
 *    pm_device_suspend(dev)  — app explicitly suspends
 *    pm_device_resume(dev)   — app explicitly resumes
 *
 * 2. Usage-counted (like Linux):
 *    pm_runtime_get(dev)     — "I'm using this device" (refcount++)
 *    pm_runtime_put(dev)     — "I'm done" (refcount--, auto-suspend at 0)
 *
 * Drivers provide suspend/resume callbacks via pm_ops.
 * The PM framework calls them at the right time.
 */

#ifndef PM_H
#define PM_H

#include "device.h"
#include <stdint.h>

enum pm_device_state {
    PM_DEVICE_ACTIVE,
    PM_DEVICE_SUSPENDED,
};

enum pm_device_action {
    PM_DEVICE_ACTION_SUSPEND,
    PM_DEVICE_ACTION_RESUME,
};

/* Driver provides this callback */
typedef int (*pm_device_action_fn)(const struct device *dev,
                                   enum pm_device_action action);

/* Per-device PM state (stored alongside the device) */
struct pm_device {
    pm_device_action_fn action_cb;
    enum pm_device_state state;
    int usage_count;  /* for runtime PM: >0 means active */
};

/* ---- Manual PM (Zephyr-style) ---- */

int pm_device_suspend(const struct device *dev, struct pm_device *pm);
int pm_device_resume(const struct device *dev, struct pm_device *pm);

/* ---- Usage-counted PM (Linux-style) ---- */

/* "I need this device" — resumes if suspended, increments refcount */
int pm_runtime_get(const struct device *dev, struct pm_device *pm);

/* "I'm done" — decrements refcount, suspends when it hits 0 */
int pm_runtime_put(const struct device *dev, struct pm_device *pm);

#endif
