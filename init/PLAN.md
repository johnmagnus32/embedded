# init — a minimal declarative init + service supervisor (PLAN)

Our own PID-1 for the gameboy-v3 stacks, replacing the shell-handoff placeholder. Selected as
`INIT=custom` (peer to `INIT=runit`); installs as `/sbin/init` + `/init`, with a control tool
`initctl`. Generic on purpose — nothing gameboy-v3-specific (that's why it's not `gv3*`).

## Decisions (settled)
- **Language: C.** The gv3 shell subset (no loops/signals/`$()`/`&&`) cannot reap, supervise, or
  handle signals — a real init has to be a program.
- **Supervisor target: MAINLINE ONLY** (`KERNEL=mainline LIBC=musl`). That's where the console
  (canvasd/appletd/powerd) runs and where the full signal + `reboot` ABI exists. On the custom
  kernel `init` runs as a minimal PID-1 (mount + reap + shell); full supervision there waits on
  kernel work — see "Custom-kernel gaps".
- **Config: declarative, one `.conf` per service** in a drop-in dir `/etc/init/` (finit/Upstart
  style; "simpler systemd"). **Service name = filename stem.**
- **No brand name:** binary `init`, tool `initctl`, config dir `/etc/init/`, control socket
  `/run/initctl.sock`.

## No modes — always read config and supervise
`init` always mounts, reads `/etc/init/*.conf`, starts + supervises whatever is there, and reaps
forever. One code path. Empty config → it idles + reaps (like runit with an empty `/etc/service`).
A shell is not special: want a debug console, ship a `shell.conf` (`respawn /bin/sh`). There is NO
"run a shell and halt when it exits" mode — no mainstream init has one (BusyBox's compiled-in default
inittab is the closest, and it *respawns* a shell rather than halting). Halt/reboot is an explicit
action (`initctl poweroff`, power button, battery-critical), never "exit the shell".

NOTE (golden): today's `exit → "last process exited; halting"` is an artifact of the placeholder
`exec /bin/sh`-as-PID-1 handoff (PID 1 exiting = kernel halt) — it disappears with ANY real init, so
it's not preserved with a mode; it's a test update at cutover. `INIT=custom` keeps the old shell
handoff through Phases A–B (golden stays green); at the Phase-C cutover the golden busybox/dynamic
cases switch to shutting down via the real path — on the custom kernel `init`'s shutdown degrades to
`sync()` + `_exit(0)` (no reboot syscall → the kernel prints the same halt marker).

## Built-in PID-1 behavior (not configurable)
Before reading any config, `init` (as PID 1):
1. Mounts the API filesystems if absent: `proc`→`/proc`, `sysfs`→`/sys`, `devtmpfs`→`/dev`,
   `tmpfs`→`/run`, `tmpfs`→`/tmp` (size-capped for the 128 MB budget). [read-only-root + extra
   tmpfs overlays: future.]
2. Opens `/dev/console` as its ctty; wires stdin/out/err.
3. Sets a base `PATH` (`/usr/sbin:/usr/bin:/sbin:/bin`) for children.
4. Reaps orphaned children forever (the defining PID-1 duty). NEVER exits until an explicit shutdown.

## Config format (`/etc/init/<name>.conf`)
Line-oriented `key value`. `#` as the first non-blank char = full-line comment; **no inline
comments** (values run verbatim to end-of-line — avoids the trailing-whitespace / comment-leak bug
we hit twice). Unknown keys warn + ignore (forward-compatible). Service name = filename stem.

| key | value | meaning | default |
|-----|-------|---------|---------|
| `exec` | command line | **REQUIRED.** Foreground process; split to argv on whitespace with single/double quotes grouping args (one layer stripped, like systemd `ExecStart`). No shell/expansion — use `exec /bin/sh -c '…'` for pipes/vars/globs. | — |
| `description` | text | label for `initctl` / logs | filename |
| `oneshot` | *(flag)* | run once to completion; don't respawn | respawn daemon |
| `after` | name… | start after these are up (oneshot dep = exited 0) | none |
| `ready` | path | mark up when this path appears (bounded wait) | up on spawn |
| `env` | `KEY=VALUE` | env var for the process (repeatable) | inherit init's |

Deferred keys (add when a service needs one — non-breaking via warn+ignore): `manual`,
`stop-signal`, `user`, `dir`, log redirection, restart tuning, `before`/`requires`.

## Supervision semantics
- **Startup:** topological by `after`. A service starts once its deps are "up" — spawned (+ `ready`
  path present, if set) for a daemon, exited 0 for a oneshot. **All waits are BOUNDED**: a dep or
  `ready` path that never arrives is logged and boot proceeds (never hangs — same rule as the
  runit socket-wait).
- **Respawn:** a non-`oneshot` service that exits is restarted after a short backoff, with a
  crash-loop guard (N deaths in T seconds → mark `failed`, stop respawning). `oneshot` exiting
  non-zero → log, don't restart.
- **Reaping:** on `SIGCHLD` via `signalfd`, then a `wait4(WNOHANG)` drain.
- **Event loop:** the supervisor blocks in `epoll` on a `signalfd` (a child exited) + a `timerfd`
  (next backoff / ready-gate deadline) — systemd-style. With nothing pending, the timer is disarmed
  and it sleeps with **zero wakeups** until a child dies. Deadlines use `CLOCK_MONOTONIC`. This makes
  the supervisor **mainline-only** (signalfd/timerfd/epoll/clock_gettime — see Custom-kernel gaps);
  it REQUIRES them: on a kernel without them, init fails loudly (die) rather than degrading — no
  fallback (fix the kernel, don't work around it here).

## Shutdown
On `initctl poweroff|reboot` (or a term signal to init): stop autostarted daemons in **reverse**
`after` order — `SIGTERM`, grace period, `SIGKILL` survivors — reap, `sync()`, then `reboot(RB_*)`.
On the custom kernel (no reboot syscall) the final step **degrades to `_exit(0)`**, which the kernel
turns into its halt — so shutdown works everywhere; only the very last instruction differs.

## initctl (control tool)
Talks to `init` over a unix socket `/run/initctl.sock`, one-line text protocol. Verbs:
`status [svc]`, `start|stop|restart <svc>`, `reload` (rescan `/etc/init/`), `poweroff`, `reboot`.

## Build / integration
- **Source:** top-level `init/` (peer to `kernel/`, `bootloader/`, `libc/`): `init.c` (+ modules),
  `initctl.c`, a Makefile; cross-built against the selected libc.
- **Provider recipe** `forge/providers/init/custom/recipe.sh`: `PKG_FETCH=local`, `PKG_SOURCE=init`,
  `PKG_DEPENDS=libc`; installs `/sbin/init`, `/init` (→ `/sbin/init`), `/usr/bin/initctl`.
- **Linkage: init FOLLOWS the rootfs `LINKAGE`** — like every other package, via the libc cc-profile
  (`PKG_DEPENDS=libc`; the Phase-C recipe sources `LIBC_CC_PROFILE` and passes `PKG_CC/CFLAGS/LDFLAGS`
  to `make -C init`, exactly as the console recipe does). This is always safe because init lives in
  the rootfs, so its linkage matches what the rootfs ships: a static rootfs (no loader) gets a static
  init; a dynamic rootfs ships `ld.so` + `libc.so`, so a dynamic init loads (the whole initramfs is
  unpacked before the kernel execs `/init` — the dynamic golden case proves PID 1 can come up through
  `ld-musl`, on both kernels). No special-casing (an earlier "always static" idea was dropped as
  over-conservative — the kernel doesn't require a static PID 1; systemd's is dynamic). The standalone
  Makefile defaults dynamic for host/dev and honors `CC`/`CFLAGS`/`LDFLAGS` overrides.
- **Console package** ships `/etc/init/{canvasd,appletd,powerd}.conf` (the Yocto-model authored
  files, now `.conf` instead of runit `run` scripts).
- The old shell placeholder (`gv3init` + its `init` script) is removed.

## Phasing
- **A — PID-1 core (C) [DONE]:** built-in mounts + console + PATH + reap-forever + never-exit + parse
  `/etc/init/*.conf`. Standalone; `INIT=custom` still ships the shell handoff, so golden is untouched.
- **B — supervision [DONE]:** `after` ordering, bounded `ready` gating, respawn + backoff + crash-guard,
  `oneshot` — and an EVENT-DRIVEN `epoll`/`signalfd`/`timerfd` loop (zero idle wakeups, `CLOCK_MONOTONIC`).
- **C — cutover + init boot test [DONE]:** `INIT=custom` now builds the C init from `init/` and
  installs `/init` (NOT `/sbin/init` — busybox owns that symlink; a real file merged over it derefs +
  clobbers busybox, which caused a fork-bomb until fixed). The old shell handoff became `INIT=shell`
  (golden's busybox/dynamic + the from-scratch stack use it; golden stays a pure kernel test, 6/6).
  New `init/test/boot.sh` boots the C init as REAL PID 1 on a mainline kernel under QEMU (`-M virt`,
  `-net none`) with the `init/test/conf/` service fixtures (spliced into /etc/init via a concatenated
  cpio — fixtures live with the test, not in the product package catalog), asserting oneshot/respawn/after/ready —
  GREEN. (Shipping the *console's* own `.conf` + booting real canvas is gated on the console
  cross-build, a separate track.) LATENT: the rootfs merge's `cp -a` derefs symlink collisions — fine
  now that /init doesn't collide, but `cp -a --remove-destination`/`-T` would be more robust.
- **D — shutdown + initctl [DONE]:** the `initctl` control socket (`/run/initctl.sock`) joins the
  `epoll` as one more fd, with the full verb set (`poweroff|reboot|halt|status|start|stop|restart|
  reload`); `SIGTERM`→poweroff / `SIGINT`→reboot route through the `signalfd`. Orderly shutdown:
  SIGTERM every service → grace → SIGKILL survivors → `sync()` → `reboot()`, degrading to `_exit(0)`
  on the custom kernel (no reboot syscall → kernel halts). Boot-proven in `init/test/boot.sh`:
  `initctl status` over the socket + `initctl poweroff` → PSCI `SYSTEM_OFF` → clean QEMU exit.
- **E — remaining:** power button + battery-critical → `initctl poweroff` (device-specific: an
  evdev/PMIC input source, deferred); optional per-service log capture (the svlogd analog).

## Custom-kernel gaps (close later, for a from-scratch-stack supervisor)
The supervisor is mainline-only until the custom kernel grows what it needs. Verified in
`kernel/arch/arm/syscall.c`: signals are STORED (`rt_sigaction`/`rt_sigprocmask`) but NOT DELIVERED
(no `do_signal`/handler frame; `rt_sigreturn` is a stub), and there is NO `reboot` syscall. On top of
that the event loop needs `signalfd`, `timerfd`, `epoll`, and `clock_gettime(CLOCK_MONOTONIC)`, none
of which the custom kernel implements — so today the C init can't run there at all (it fails loudly
via `die` if they're missing; the custom kernel uses `INIT=shell` instead). Closing the gap = (1) signal delivery, (2) `sys_reboot`,
(3) `signalfd`/`timerfd`/`epoll` + a monotonic clock. Deliberately NOT bandaided with a poll fallback
(USER: fix the kernel, don't work around it elsewhere).
