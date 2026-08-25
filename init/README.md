# init — minimal declarative init + service supervisor

Our own PID 1 (`INIT=custom`). Installs as `/init` (the initramfs entry point the kernel execs —
not `/sbin/init`, which busybox owns as a symlink to itself). Reads one declarative `.conf` per
service from `/etc/init/` and supervises them. Design + full grammar in [PLAN.md](PLAN.md). Generic
on purpose — nothing gameboy-v3-specific.

## Build
    make                                             # host build -> build/init
    make CC=arm-buildroot-linux-musleabihf-gcc       # cross-build for the board (musl)
    make DESTDIR=/tmp/root install                   # -> /sbin/init + /init symlink

## Status: Phase D done — supervisor + shutdown + initctl, boot-tested on mainline
Done: best-effort early setup (mount proc/sys/dev/run/tmp, take /dev/console, set PATH); parse
`/etc/init/*.conf`; a supervision loop — `after` ordering, bounded `ready` gating, respawn + backoff
+ crash-loop guard, `oneshot` (DONE/FAILED); reap orphans forever; never exit. EVENT-DRIVEN: blocks
in `epoll` on `signalfd`+`timerfd` (`CLOCK_MONOTONIC`), zero wakeups when steady — hence mainline-only
(it REQUIRES them — a kernel lacking those makes init fail loudly via `die`, no fallback; that kernel
uses `INIT=shell`). **Cutover done:** `INIT=custom` builds this from
`init/` and installs `/init`; the minimal shell PID-1 is now `INIT=shell` (golden + the from-scratch
stack use it). **Shutdown + control done:** `SIGTERM`→poweroff / `SIGINT`→reboot; the `initctl`
control tool (`/run/initctl.sock`) with `poweroff|reboot|halt|status|start|stop|restart|reload`;
orderly shutdown (SIGTERM→grace→SIGKILL→`sync`→`reboot()`, or `_exit(0)` where there's no reboot syscall).

## Tests
- `make` (host) — compile check (host build is dynamic; `make CC=<cross> LDFLAGS=-static` = shipped form).
- `init/test/boot.sh` — boots this as REAL PID 1 on a mainline kernel under QEMU `-M virt`
  (`KVIRT_ZIMAGE` selects the kernel) with the `init/test/conf/*.conf` fixtures (spliced into /etc/init
  via a concatenated cpio — they live with the test, not in the product package catalog); asserts oneshot / respawn
  / after-ordering / ready-gating. This is the swappability proof (custom init vs the OSS baseline).

Not yet (Phase E): power button + battery-critical → `initctl poweroff` (device-specific); optional
per-service log capture.
