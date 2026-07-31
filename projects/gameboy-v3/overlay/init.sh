#!/bin/sh
# /init — PID 1 for gameboy-v3, PORTABLE across EVERY rootfs+kernel combo.
#
# ONE init, shipped regardless of the LIBC/COREUTILS provider — because PID-1
# policy is the PRODUCT's, not the provider's. It runs identically on:
#   * busybox + musl + mainline Linux  (mount actually mounts; full shell)
#   * our coreutils + libc + custom kernel  (mount/setsid are kernel no-op
#     successes; our shell parses this subset)
#
# The kernel extracts the initramfs cpio and execs /init as PID 1; it must NOT
# return (a PID 1 that exits panics the kernel), so we end in `exec /bin/sh`.
#
# WRITTEN IN THE PORTABLE SHELL SUBSET (our gv3 shell, coreutils/sh.c, splits on
# whitespace only — NO quoting, NO $(...), NO &&/||/if). So:
#   * echo lines are unquoted (multiple spaces collapse; that's fine).
#   * NO command substitution — the live banner ($(uname)/$(nproc)) is dropped in
#     favour of a static line that works on both shells.
#   * mounts are best-effort: on mainline they populate /proc,/sys,/dev; on the
#     custom kernel SYS_mount returns 0 as a no-op (see kernel arch/arm/syscall.c)
#     so the dirs stay empty but init proceeds. If a `mount` BINARY is absent
#     (our coreutils has none yet), the fork just prints "not found" and the shell
#     keeps going — a non-exec failure is non-fatal.
#   * hand off with `exec /bin/sh` (NOT `exec setsid cttyhack /bin/sh`): on the
#     gv3 shell a failed exec would STOP pid 1 and panic; and the custom kernel
#     already wires /dev/console stdio for PID 1. busybox/mainline only forgo the
#     controlling-tty niceties (Ctrl-C/job control at PID 1) — an acceptable,
#     honest trade for one portable init. (A capability-probed init that keeps
#     setsid+cttyhack on busybox is possible once the gv3 shell grows command -v
#     + &&/|| — see the note in kernel/coreutils docs.)

# Essential virtual filesystems (real on mainline; no-op success on the custom
# kernel; harmless "not found" if no mount binary is shipped).
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
