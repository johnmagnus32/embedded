#!/usr/bin/env bash
# golden.sh — the regression safety net for the gameboy-v3 kernel.
#
# WHY THIS EXISTS: we are about to make invasive, race-prone changes (real fault
# handling, sleep/wakeup queues, and eventually a preemptive scheduler). Each of
# those should be a NO-OP to observable boot behavior until the very last step.
# This harness turns "does the kernel still boot to a working userspace?" into a
# single automated pass/fail, so any regression is caught in isolation.
#
# It runs the kernel under QEMU `-M virt` and checks program OUTPUT (not QEMU's
# exit code — the kernel halts in `wfi` and QEMU never exits on its own, so we
# detect completion by a terminal marker and then kill QEMU).
#
# Two cases:
#   smoke  — the built-in test initramfs (uinit): deterministic, needs NO input,
#            ALWAYS available. Exercises fs + mem syscalls + fork/exec/wait.
#   busybox — the real musl BusyBox rootfs, driven with a fixed command script to
#            an interactive prompt. Only runs if the image is present (it's a
#            build artifact of the busybox rootfs (make image ... from the product)); otherwise SKIPPED.
#
# Usage:
#   ./test/golden.sh                 # build (BOARD=virt) + run both cases
#   ./test/golden.sh smoke           # only the built-in smoke test (fast)
#   ./test/golden.sh busybox         # only the real-BusyBox interactive test
#   NO_BUILD=1 ./test/golden.sh      # skip the build, use existing artifacts
#
# Exit code: 0 iff every case that ran PASSED. Logs saved under build/test/.

set -u

# ---- locate ourselves + the kernel dir --------------------------------------
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KDIR="$(cd "${HERE}/.." && pwd)"                 # <repo>/kernel (a top-level PROVIDER since the forge refactor)
REPO_ROOT="$(cd "${KDIR}/.." && pwd)"            # <repo> (= 'embedded/')
# The kernel is now a top-level provider, but the golden test still consumes a
# PRODUCT's build artifacts (toolchain + busybox initramfs). Point at the
# gameboy-v3 product. (Phase 4 would parameterize this; for now name it.)
PROJ="${GV3_PRODUCT:-${REPO_ROOT}/projects/gameboy-v3}"
# The kernel builds per-board under build/<board>/; the golden test always runs
# the QEMU virt build.
BUILD="${KDIR}/build/virt"
LOGDIR="${KDIR}/build/test"
TOOLCHAIN_BIN="${PROJ}/build/toolchain/bin"
BUSYBOX_INITRD="${PROJ}/build/output/initramfs.cpio.gz"
DYNAMIC_INITRD="${PROJ}/build/output/initramfs-dynamic.cpio.gz"
SMOKE_INITRD="${BUILD}/initramfs.cpio.gz"
FAULT_INITRD="${BUILD}/faultramfs.cpio.gz"
ORPHAN_INITRD="${BUILD}/orphanramfs.cpio.gz"
PREEMPT_INITRD="${BUILD}/preemptramfs.cpio.gz"
KIMG="${BUILD}/gv3kernel.bin"

# put the cross toolchain on PATH (the Makefile needs $(CROSS_COMPILE)gcc)
case ":${PATH}:" in *":${TOOLCHAIN_BIN}:"*) : ;; *) PATH="${TOOLCHAIN_BIN}:${PATH}" ;; esac
export PATH

QEMU="${QEMU:-qemu-system-arm}"

red()   { printf '\033[31m%s\033[0m\n' "$*"; }
grn()   { printf '\033[32m%s\033[0m\n' "$*"; }
ylw()   { printf '\033[33m%s\033[0m\n' "$*"; }
info()  { printf '  %s\n' "$*"; }

fail_count=0
ran_count=0

# ---- one test case ----------------------------------------------------------
# run_case <name> <initrd> <timeout_s> <feeder_fn|none>
# Requires globals REQ[] (all must appear) and FORB[] (none may appear) to be set
# by the caller. Writes the transcript to $LOGDIR/<name>.log.
run_case() {
  local name="$1" initrd="$2" tmout="$3" feeder="$4"
  local log="${LOGDIR}/${name}.log"

  printf '\n=== case: %s ===\n' "$name"
  ran_count=$((ran_count + 1))
  : "${EXTRA_CHECK:=}"          # honored below; caller sets it before run_case

  # One QEMU boot. QEMU 5.2's TCG occasionally stalls at startup and emits NOTHING
  # (a known host flake, not a kernel bug — the kernel is deterministic). Detect a
  # zero-output run and retry ONCE so the flake doesn't cause a false failure; a
  # real hang still fails (it produces boot output but never the halt marker).
  local done=0 attempt
  for attempt in 1 2; do
    local fifo; fifo="$(mktemp -u "${TMPDIR:-/tmp}/gv3-${name}.XXXXXX")"
    mkfifo "$fifo" || { red "  cannot create fifo"; return 1; }
    # Own the fifo read+write on fd 9 so opening never blocks and QEMU never sees
    # EOF on the console until we explicitly close it.
    exec 9<>"$fifo"
    "$QEMU" -M virt -cpu cortex-a7 -m 128M -nographic -net none \
      -kernel "$KIMG" -initrd "$initrd" <"$fifo" >"$log" 2>&1 &
    local qpid=$!
    local fpid=""
    if [ "$feeder" != none ]; then "$feeder" >&9 & fpid=$!; fi

    local waited=0; done=0
    while [ "$waited" -lt "$tmout" ]; do
      if grep -qF 'last process exited; halting' "$log" 2>/dev/null; then done=1; break; fi
      kill -0 "$qpid" 2>/dev/null || { done=1; break; }   # QEMU exited by itself
      sleep 1; waited=$((waited + 1))
    done

    [ -n "$fpid" ] && kill "$fpid" 2>/dev/null
    exec 9>&-                                   # close our write end
    kill "$qpid" 2>/dev/null; wait "$qpid" 2>/dev/null
    rm -f "$fifo"

    # Retry only on the empty-output stall (QEMU never even printed the banner).
    if [ "$done" -eq 0 ] && [ ! -s "$log" ] && [ "$attempt" -eq 1 ]; then
      ylw "  (no output — likely a QEMU startup stall; retrying once)"
      sleep 2; continue
    fi
    break
  done

  # ---- evaluate ----
  local bad=0
  if [ "$done" -eq 0 ]; then
    red "  TIMEOUT after ${tmout}s (never reached kernel halt)"; bad=1
  fi
  local m
  for m in "${REQ[@]}"; do
    if ! grep -qF -- "$m" "$log"; then red "  MISSING required: ${m}"; bad=1; fi
  done
  for m in "${FORB[@]}"; do
    if grep -qF -- "$m" "$log"; then red "  FORBIDDEN present: ${m}"; bad=1; fi
  done
  # Optional custom predicate: EXTRA_CHECK names a function taking the log path;
  # nonzero return => failure. Lets a case assert richer properties than markers
  # (e.g. output interleaving). Cleared by the caller each time.
  if [ -n "${EXTRA_CHECK:-}" ]; then
    if ! "$EXTRA_CHECK" "$log"; then red "  EXTRA_CHECK failed: ${EXTRA_CHECK}"; bad=1; fi
  fi

  if [ "$bad" -eq 0 ]; then
    grn "  PASS  (log: ${log#${PROJ}/})"
  else
    red "  FAIL  (full transcript: ${log})"
    fail_count=$((fail_count + 1))
  fi
}

# ---- feeders (timed console input) ------------------------------------------
# The shell only starts reading ~1-2s into boot; pace input so it isn't dropped.
feed_busybox() {
  sleep 4
  printf 'echo GOLDEN-SMOKE-OK\n';                      sleep 1
  printf 'uname -a\n';                                  sleep 1
  printf 'echo sub: $(echo hi | tr a-z A-Z)\n';         sleep 1
  printf 'exit\n';                                      sleep 1
}

# ---- markers for each case --------------------------------------------------
smoke_case() {
  REQ=(
    'mmu selftest:'                                   # MMU translation works
    'readback MATCH (translation works)'
    "proc: spawned '/init' pid 1"
    'init: brk grow + heap write/read OK'
    'init: mmap2 anon page write/read OK'
    'init: munmap OK'
    'init: TIOCGWINSZ ok, 24x80 (isatty path works)'
    'hello: argc=2'
    'hello:   argv[1] = arg1'
    'hello:   env: HOME=/'
    'init: child pid 2 exited with code 7'
    'init: S9 filesystem test complete, exiting 0'
    '[kernel] last process exited; halting.'
  )
  FORB=(
    'MISMATCH'
    'FAILED'
    'unimplemented syscall'
    '-ENOSYS'
    '*** '                                            # a fault_report() spin
    'open dir failed'
    'open file failed'
  )
  run_case smoke "$SMOKE_INITRD" 20 none
}

fault_case() {
  # Proves fault ISOLATION: a child writes through NULL (Data Abort); the kernel
  # must KILL that process (not spin the whole machine) and the parent survives.
  # TWO rounds: the second fault also proves the abort-mode banked SP is reset on
  # every fault entry (regression test for the "reset only on first fault" bug).
  REQ=(
    'faultinit: I am pid 1'
    'fault: about to write through a NULL pointer'
    'killed by data-abort'                            # kernel's fault report
    'faultinit: PARENT SURVIVED round 1. child pid 2 killed by signal 11'
    'faultinit: PARENT SURVIVED round 2. child pid 3 killed by signal 11'
    'faultinit: fault-isolation test complete, exiting 0'
    '[kernel] last process exited; halting.'
  )
  FORB=(
    'SURVIVED the null write'                          # child must NOT run past the fault
    'KERNEL PANIC'                                     # a USER fault must not panic
    'unimplemented syscall'
  )
  run_case fault "$FAULT_INITRD" 20 none
}

orphan_case() {
  # Proves orphan REPARENTING *and* the slot-reuse edge: A forks B then exits;
  # B blocks on a pipe (stays alive, orphaned); init reaps A, forks N (which
  # REUSES A's freed slot), reaps N, then wakes+reaps B. With the stale-parent-
  # pointer bug, B's parent would alias N and B would be lost (-ECHILD -> FAIL).
  REQ=(
    'orphaninit: pid 1 up'
    'orphaninit: [A] forked B, exiting WITHOUT waiting.'
    'orphaninit: [B] blocking on pipe (alive, orphaned).'
    'orphaninit: [B] woken; exiting.'
    'orphaninit: REPARENT+SLOTREUSE OK (all of A,N,B reaped correctly)'
    '[kernel] last process exited; halting.'
  )
  FORB=(
    'orphaninit: REPARENT FAILED'
    'wait4 -> -ECHILD (a child was lost!)'
    'KERNEL PANIC'
    'killed by'
  )
  run_case orphan "$ORPHAN_INITRD" 20 none
}

# Predicate: the two CPU-bound children's markers must INTERLEAVE — i.e. a [B]
# appears before the LAST [A] (and an [A] before the last [B]). If A ran to
# completion before B (cooperative behavior), all [A]s precede all [B]s and this
# fails. Interleaving can only come from the timer preempting a syscall-free loop.
check_preempt_interleave() {
  local log="$1"
  local first_b last_a first_a last_b clean
  # The kernel emits CRLF; strip CRs to a temp so anchored matches work portably
  # (some greps here are ugrep, which rejects \r? in the pattern).
  clean="$(mktemp "${TMPDIR:-/tmp}/gv3-il.XXXXXX")"
  tr -d '\r' < "$log" > "$clean"
  first_a=$(grep -n '^\[A\]$' "$clean" | head -1 | cut -d: -f1)
  last_a=$(grep -n '^\[A\]$' "$clean" | tail -1 | cut -d: -f1)
  first_b=$(grep -n '^\[B\]$' "$clean" | head -1 | cut -d: -f1)
  last_b=$(grep -n '^\[B\]$' "$clean" | tail -1 | cut -d: -f1)
  rm -f "$clean"
  if [ -z "$first_a" ] || [ -z "$first_b" ]; then
    red "    (interleave: missing [A] or [B] markers)"; return 1
  fi
  # Interleaved iff a B precedes the last A AND an A precedes the last B.
  if [ "$first_b" -lt "$last_a" ] && [ "$first_a" -lt "$last_b" ]; then
    return 0
  fi
  red "    (interleave: markers are segregated — no time-slicing? first_b=$first_b last_a=$last_a first_a=$first_a last_b=$last_b)"
  return 1
}

preempt_case() {
  # Proves the timer PREEMPTS user code: two CPU-bound children (no syscalls in
  # their loops) must time-slice. Markers must interleave (see predicate) and
  # both children must finish. Cooperative scheduling cannot produce this.
  REQ=(
    'preemptinit: forking two CPU-bound children'
    'preemptinit: BOTH CPU-bound children finished (preemption works)'
    '[kernel] last process exited; halting.'
  )
  FORB=(
    'preemptinit: a child was lost'
    'KERNEL PANIC'
    'killed by'
  )
  EXTRA_CHECK=check_preempt_interleave
  run_case preempt "$PREEMPT_INITRD" 40 none
  EXTRA_CHECK=
}

busybox_case() {
  if [ ! -f "$BUSYBOX_INITRD" ]; then
    ylw "=== case: busybox === SKIPPED (no ${BUSYBOX_INITRD#${PROJ}/}; run: make image KERNEL=mainline LIBC=musl COREUTILS=busybox (from projects/gameboy-v3))"
    return 0
  fi
  REQ=(
    'T113-S3 is alive.'
    'Linux 0.9-gv3 armv7l'                            # $(uname -srm) substitution
    'cores online: 1'                                 # $(nproc) substitution
    'Linux gameboy-v3 0.9-gv3 gv3kernel S10 armv7l GNU/Linux'   # uname -a output
    'sub: HI'                                         # $(echo hi | tr ...) over a pipe
    '[kernel] last process exited; halting.'
  )
  FORB=(
    'unimplemented syscall'
    'MISMATCH'
    '*** '
  )
  run_case busybox "$BUSYBOX_INITRD" 30 feed_busybox
}

dynamic_case() {
  # DYNAMICALLY-linked musl BusyBox: proves the kernel's dynamic-linking support
  # (ET_DYN load bias, PT_INTERP -> /lib/ld-musl-armhf.so.1, full auxv, file-backed
  # + MAP_FIXED mmap2). Same shell interactions as busybox, but the whole chain
  # runs THROUGH ld.so. Build with: LINKAGE=dynamic make (from projects/gameboy-v3/rootfs) — see rootfs/README
  if [ ! -f "$DYNAMIC_INITRD" ]; then
    ylw "=== case: dynamic === SKIPPED (no ${DYNAMIC_INITRD#${PROJ}/}; run LINKAGE=dynamic make (from projects/gameboy-v3/rootfs) — see rootfs/README)"
    return 0
  fi
  REQ=(
    'pid 1 pc 0x2'                                    # PID 1 entered at ld.so (INTERP_BASE 0x20000000)
    '[dynamic]'                                        # kernel took the dynamic path
    'T113-S3 is alive.'
    'Linux 0.9-gv3 armv7l'                            # $(uname -srm) via a forked dynamic applet
    'cores online: 1'                                 # $(nproc)
    'Linux gameboy-v3 0.9-gv3 gv3kernel S10 armv7l GNU/Linux'   # uname -a
    'sub: HI'                                         # $(echo hi | tr ...) over a pipe
    '[kernel] last process exited; halting.'
  )
  FORB=(
    'unimplemented syscall'
    'interpreter'"'"' not found'                       # ld.so must be found
    'interp not ET_DYN'
    'MISMATCH'
    '*** '                                             # a fault/panic
  )
  run_case dynamic "$DYNAMIC_INITRD" 30 feed_busybox
}

# ---- main -------------------------------------------------------------------
main() {
  command -v "$QEMU" >/dev/null 2>&1 || { red "error: ${QEMU} not on PATH"; exit 2; }
  mkdir -p "$LOGDIR"

  if [ "${NO_BUILD:-0}" != 1 ]; then
    printf '=== building (BOARD=virt) ===\n'
    if ! make -C "$KDIR" BOARD=virt >"${LOGDIR}/build.log" 2>&1; then
      red "BUILD FAILED (see ${LOGDIR}/build.log)"; tail -n 20 "${LOGDIR}/build.log"; exit 2
    fi
    grn "  build OK"
  fi
  [ -f "$KIMG" ] || { red "error: ${KIMG} missing (build first, or unset NO_BUILD)"; exit 2; }

  local want="${1:-all}"
  case "$want" in
    smoke)   smoke_case ;;
    fault)   fault_case ;;
    orphan)  orphan_case ;;
    preempt) preempt_case ;;
    busybox) busybox_case ;;
    dynamic) dynamic_case ;;
    all)     smoke_case; fault_case; orphan_case; preempt_case; busybox_case; dynamic_case ;;
    *)       red "usage: $0 [smoke|fault|orphan|preempt|busybox|dynamic|all]"; exit 2 ;;
  esac

  printf '\n=== summary ===\n'
  if [ "$fail_count" -eq 0 ]; then
    grn "ALL GREEN — ${ran_count} case(s) ran, 0 failed."
    exit 0
  else
    red "${fail_count} of ${ran_count} case(s) FAILED."
    exit 1
  fi
}

main "$@"
