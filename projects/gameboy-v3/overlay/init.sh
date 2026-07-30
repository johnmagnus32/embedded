#!/bin/sh
# init.sh — PID 1 for the gv3 from-scratch rootfs, as a SHELL SCRIPT.
#
# The kernel's ELF loader sees the `#!/bin/sh` shebang, launches /bin/sh with
# argv = ["/bin/sh", "/init"], and our shell runs this file (script mode). The
# final `exec` REPLACES the shell-running-the-script with a fresh interactive
# shell, so PID 1 becomes that interactive shell and never exits (a PID 1 that
# returns would panic the kernel). Mirrors the reference busybox init's shape
# (`exec ... /bin/sh`) using only features our shell/kernel have — no mount,
# no setsid/cttyhack (our kernel wires /dev/console stdio for PID 1 already).
#
# NOTE: our shell has no quote handling yet (it splits on whitespace), so these
# echo lines are intentionally unquoted. Multiple spaces collapse to one.
echo
echo ===================================================
echo gameboy-v3 rootfs -- from scratch, gv3libc
echo PID 1 is a shell script, handing off to /bin/sh
echo ===================================================
echo
exec /bin/sh
