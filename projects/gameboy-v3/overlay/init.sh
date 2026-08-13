#!/bin/sh
# /init — PID 1 for gameboy-v3, ONE portable init for EVERY rootfs+kernel combo (PID-1
# policy is the product's, not the provider's). Runs identically on busybox+musl+mainline
# and on our coreutils+libc+custom kernel.
#
# TWO hard constraints:
#  1. PID 1 must NOT return — a PID 1 that exits panics the kernel — so we end in `exec /bin/sh`.
#  2. Written in the PORTABLE shell subset the gv3 shell (coreutils/sh.c) parses: splits on
#     whitespace only — NO quoting, NO $(...), NO &&/||/if. So the banner is static (no live
#     $(uname)), and we hand off with a bare `exec /bin/sh` (not setsid+cttyhack — a failed
#     exec would panic PID 1 on the gv3 shell, and the custom kernel already wires
#     /dev/console stdio; the only cost is job control at PID 1 on busybox/mainline).

# Virtual filesystems — best-effort: real on mainline; a no-op success on the custom kernel
# (SYS_mount returns 0); a harmless "not found" if no mount binary is shipped.
mount -t proc     none /proc
mount -t sysfs    none /sys
mount -t devtmpfs none /dev

echo
echo ===================================================
echo   gameboy-v3 -- PID 1 is alive.
echo   handing off to an interactive /bin/sh
echo ===================================================
echo

# Replace PID 1 with the interactive shell so we never return to the kernel.
exec /bin/sh
