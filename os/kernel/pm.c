/*
 * pm.c — Per-device runtime power management
 *
 * Linux equivalent: drivers/base/power/runtime.c
 * Zephyr equivalent: subsys/pm/device_runtime.c
 */

#include "config.h"

#ifdef CONFIG_PM

#include "pm.h"

/* ---- Manual PM ---- */

int pm_device_suspend(const struct device *dev, struct pm_device *pm)
{
    if (pm->state == PM_DEVICE_SUSPENDED)
        return 0;  /* already suspended */

    if (pm->action_cb) {
        int rc = pm->action_cb(dev, PM_DEVICE_ACTION_SUSPEND);
        if (rc < 0) return rc;
    }

    pm->state = PM_DEVICE_SUSPENDED;
    return 0;
}

int pm_device_resume(const struct device *dev, struct pm_device *pm)
{
    if (pm->state == PM_DEVICE_ACTIVE)
        return 0;  /* already active */

    if (pm->action_cb) {
        int rc = pm->action_cb(dev, PM_DEVICE_ACTION_RESUME);
        if (rc < 0) return rc;
    }

    pm->state = PM_DEVICE_ACTIVE;
    return 0;
}

/* ---- Usage-counted PM (Linux-style) ---- */

int pm_runtime_get(const struct device *dev, struct pm_device *pm)
{
    pm->usage_count++;

    /* First user — resume the device */
    if (pm->usage_count == 1 && pm->state == PM_DEVICE_SUSPENDED) {
        return pm_device_resume(dev, pm);
    }

    return 0;
}

int pm_runtime_put(const struct device *dev, struct pm_device *pm)
{
    if (pm->usage_count <= 0)
        return -1;  /* not in use */

    pm->usage_count--;

    /* Last user released — suspend the device */
    if (pm->usage_count == 0) {
        return pm_device_suspend(dev, pm);
    }

    return 0;
}

#endif /* CONFIG_PM */
