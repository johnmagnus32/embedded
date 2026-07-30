/*
 * fdt.h — minimal flattened-device-tree helper for the gameboy-v3 bootloader.
 * Not a full libfdt, but enough to set /chosen/bootargs the RIGHT way: we grow
 * an existing value or insert the property if absent, fixing up the header
 * offsets and moving the blob's tail (no allocator — the DTB is loaded with
 * slack after it). This means the shared DTB no longer needs a reserved
 * placeholder bootargs string; every consumer (this bootloader, U-Boot, the
 * kernel) works off a clean/empty bootargs.
 */
#ifndef GV3_FDT_H
#define GV3_FDT_H

#include <stdint.h>

/* Validate the FDT magic at `dtb`. Returns 0 if it looks like a real blob. */
int fdt_check(const void *dtb);

/*
 * Set /chosen/bootargs to `cmdline` (NUL-terminated). Overwrites in place when
 * it fits, grows the property when the new string is longer, or inserts a new
 * bootargs property (and its "bootargs" strings-block entry) when absent — so
 * no compile-time placeholder is required. The DTB buffer must have room to
 * grow (caller loads it with slack, e.g. DTB_MAX >> blob size). Returns 0 on
 * success; negative on bad magic, unexpected layout, or /chosen missing.
 */
int fdt_set_bootargs(void *dtb, const char *cmdline);

#endif /* GV3_FDT_H */
