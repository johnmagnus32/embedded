/*
 * device.h — Simplified Zephyr-style device model
 *
 * In Zephyr, struct device is the central abstraction. Every hardware
 * peripheral is a struct device with:
 *   - config: const data from device tree (addresses, pins, etc.)
 *   - data: mutable runtime state
 *   - api: ops struct (function pointers)
 *
 * DEVICE_DT_DEFINE() creates one at compile time.
 * DEVICE_DT_GET() retrieves it by DT node label.
 */

#ifndef DEVICE_H
#define DEVICE_H

#include <stddef.h>

struct device;  /* forward declaration */

typedef int (*device_init_fn)(const struct device *dev);

struct device {
    const char *name;
    const void *config;     /* const config from device tree */
    void *data;             /* mutable runtime state */
    const void *api;        /* driver ops struct */
    device_init_fn init;    /* called at boot */
};

/*
 * DEVICE_DT_DEFINE(label, init_fn, data, config, api)
 *
 * Creates a struct device in a special ELF section so the boot code
 * can find and init all devices. Same pattern as your module_init()
 * in test-modules.
 */
#define _DEVICE_DT_DEFINE(label, init_fn, _data, _config, _api) \
    const struct device __device_##label                        \
        __attribute__((used, section("device_area"))) = {       \
            .name = #label,                                     \
            .config = (_config),                                \
            .data = (_data),                                    \
            .api = (_api),                                      \
            .init = (init_fn),                                  \
        }
/* Extra indirection so macro-expanded labels get pasted correctly */
#define DEVICE_DT_DEFINE(label, init_fn, _data, _config, _api) \
    _DEVICE_DT_DEFINE(label, init_fn, _data, _config, _api)

/*
 * DEVICE_DT_GET(label) — get a pointer to the device struct.
 * Resolves at compile time, no runtime lookup.
 */
#define _DEVICE_DT_GET(label) (&__device_##label)
#define DEVICE_DT_GET(label) _DEVICE_DT_GET(label)

/* Declare an extern device (for use in other files) */
#define _DEVICE_DT_DECLARE(label) \
    extern const struct device __device_##label
#define DEVICE_DT_DECLARE(label) _DEVICE_DT_DECLARE(label)

/*
 * DT_INST_FOREACH_STATUS_OKAY(compat, macro)
 *
 * Calls macro(n) for each instance of a compatible with status="okay".
 * C macros can't loop, so we use a dispatch trick: the code generator
 * emits DT_INST_<compat>_FOREACH which expands to macro(0) macro(1) ...
 *
 * Zephyr does the same thing with UTIL_LISTIFY and a max instance count.
 */
#define DT_INST_FOREACH_STATUS_OKAY(compat, macro) \
    DT_INST_##compat##_FOREACH(macro)

#endif
