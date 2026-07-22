/*
 * fdt.h — minimal, read-mostly flattened-device-tree helper for the gameboy-v3
 * bootloader. We do NOT implement a full libfdt: our DTB is built (at Step 2)
 * with a `/chosen/bootargs` placeholder and a `/memory` node already present,
 * so the only runtime edit needed is to overwrite the bootargs string value in
 * place (same slot, no structural change). This keeps Stage 4 tiny.
 */
#ifndef GV3_FDT_H
#define GV3_FDT_H

#include <stdint.h>

/* Validate the FDT magic at `dtb`. Returns 0 if it looks like a real blob. */
int fdt_check(const void *dtb);

/*
 * Overwrite the value of /chosen/bootargs with `cmdline` (NUL-terminated).
 * The compile-time placeholder must be >= strlen(cmdline)+1. Returns 0 on
 * success, negative if the property isn't found or won't fit.
 */
int fdt_set_bootargs(void *dtb, const char *cmdline);

#endif /* GV3_FDT_H */
