/*
 * kfdt.h — a tiny flattened-device-tree READER for the kernel's boot handoff.
 *
 * The bootloader (or U-Boot) hands us r2 = DTB physical address per the ARM
 * Linux boot protocol. The bootloader's own fdt.c is a WRITER (it patches
 * /chosen/bootargs before jumping); this is the mirror-image READER: validate
 * the blob, fetch a property value by node path + name, and pull the two things
 * S10 wants from the handoff — the initramfs location and the RAM size.
 *
 * All FDT multi-byte fields are BIG-endian regardless of CPU endianness. We
 * target version 17 (what dtc emits); the header fields we read (magic,
 * off_dt_struct, off_dt_strings, totalsize) exist in all versions >= 3.
 */
#ifndef GV3K_KFDT_H
#define GV3K_KFDT_H

#include <stdint.h>

/* Validate the FDT magic at `dtb`. Returns nonzero (1) if it looks like a real
 * blob, 0 otherwise (NULL, or wrong magic). */
int kfdt_valid(const void *dtb);

/* Total size of the blob in bytes (from the header) — for reserving it. */
uint32_t kfdt_totalsize(const void *dtb);

/* Find property `name` under absolute node path `path` (e.g. "/chosen",
 * "/memory"). On success returns a pointer to the value bytes and writes the
 * value length to *len_out; returns NULL if the node or property is absent.
 * The returned pointer is into the DTB (valid as long as the blob is). */
const void *kfdt_getprop(const void *dtb, const char *path, const char *name,
                         uint32_t *len_out);

/* Parse the kernel command line (/chosen/bootargs) for `initrd=<hexaddr>,<decsize>`.
 * On success writes the base + size and returns 0; returns -1 if absent. */
int kfdt_initrd(const void *dtb, uint32_t *base_out, uint32_t *size_out);

/* Read /memory's `reg` (assuming root #address-cells=1/#size-cells=1): the RAM
 * base + size. Returns 0 on success, -1 if absent/unsupported cell sizes. */
int kfdt_memory(const void *dtb, uint32_t *base_out, uint32_t *size_out);

#endif /* GV3K_KFDT_H */
