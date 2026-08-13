# Graduating gv3libc into its own toolchain (future work)

**Status:** NOT STARTED — design/scoping only. Captured so it can be picked up later.

**One-line goal:** make `<triple>-gcc hello.c` produce a correct gv3libc binary with NO special
flags — i.e. gv3libc becomes the *default* libc of a real toolchain, the way musl is for the
Bootlin `arm-buildroot-linux-musleabihf-` toolchain we borrow today.

## 0. Why this is even a question

gv3libc today is a **from-scratch libc riding a foreign toolchain**. We use the Bootlin *musl*
gcc/as/ld purely as a bare driver via `-nostdlib`, and hand-feed everything gcc would normally
supply automatically — see `libc/libc-profile.sh`:
- `-nostdlib -nostartfiles -ffreestanding -fno-builtin` — switch OFF gcc's automatic crt + libc.
- inject our own `crt0.S.o` as the startup object.
- `-I libc/include -I <staged UAPI> -I libc/src` — point at our headers by hand.
- `libc.a` / `-L<dir> -lc` — point at our libc by hand.
- static: `-T user.ld` (ET_EXEC @ 0x10000). dynamic: `-Wl,--dynamic-linker=/lib/ld-gv3.so.1`.

That `-nostdlib` bare-driver model is *the* non-conformance. It's why:
- `forge/core/lib/ccprofile.sh` has a per-libc branch (musl needs none of the above; gv3 needs
  all of it) — the branch can't be deleted while gv3 rides musl's toolchain.
- a sysroot alone does NOT fix it: `--sysroot` only re-roots *search paths*; it does not stop
  the musl driver auto-adding musl's crt/`-lc`/`--dynamic-linker=/lib/ld-musl-armhf.so.1`. To
  suppress those you still need `-nostdlib` + injected crt + explicit interp — i.e. the same
  flags, relocated into a cryptic gcc `.specs` file. (Investigated 2026-08-07; net-negative.)

The ONLY way to make gv3's static/dynamic "just a `-static` toggle" like musl — and to delete
ccprofile's branch honestly — is for gv3 to be its OWN toolchain: a gcc whose *baked-in specs*
default to gv3's crt, libc, headers, and interpreter, so `-nostdlib` is unnecessary.

**Important caveat before doing any of this:** gv3libc exists to be a from-scratch libc you can
read end-to-end. A from-source gcc/binutils bootstrap is a large artifact that dwarfs the libc
and is its own discipline (crosstool-NG exists precisely because this is hard). This graduation
buys ABI-conformance + a one-branch cleanup at the cost of a compiler build. Nothing outside the
gameboy-v3 rootfs consumes gv3libc, so the conformance has no external customer. Do this because
you *want* gv3 to be a real toolchain (a legitimate learning goal), not as a build-system cleanup
— the cleanup alternative (a `gv3-cc` wrapper hiding the flags behind a conforming `cc`
interface) gets the engine 100% uniform for a tiny fraction of the effort and risk.

## 1. The three tiers (ascending cost) — Tier 3 is "the full monty"

| Tier | What it is | Removes `-nostdlib`? | Cost | Verdict |
|---|---|---|---|---|
| **1** | gv3 **sysroot + gcc `-specs` file** (keep musl's gcc/as/ld binaries) | No — specs still injects crt/interp; `-nostdlib` stays in spirit | days | net-negative (moves flags into cryptic specs; ISA-freeze + foot-gun) |
| **2** | **Complete libc port**: full crt suite + libgcc + standard 5-arg `__libc_start_main`, gcc `--with-sysroot` | Yes | 1–2 wk | the minimal *real* fix; edits the crt↔kernel entry contract (silicon-critical) |
| **3** | **gcc + binutils built for an `arm-gv3-linux-gnueabihf` triple** | Yes, fully | weeks | the true "its own toolchain"; Tier 2 + a from-source gcc bootstrap |

Tiers are cumulative: **Tier 3 = Tier 2 + building the compiler.** You cannot skip Tier 2 —
a real toolchain needs the complete crt/libgcc/entry-ABI regardless of how gcc is built.

## 2. Tier 2 — the prerequisite libc-port work (must happen first)

These are gv3-side changes that make gv3 a *complete* libc, independent of how gcc is built.

### 2.1 A standard crt suite (not one crt0)
Real toolchains link a set, in order: `crt1.o` (or `Scrt1.o` for PIE) → `crti.o` → `crtbegin.o`
→ *your objects* → `crtend.o` → `crtn.o`. Today gv3 has ONE `crt0.S.o` doing the whole job.
- Split gv3's startup into `crt1.o` (`_start` + the `__libc_start_main` call) + `crti.o`/`crtn.o`
  (the `.init`/`.fini` section prologue/epilogue frames gcc expects to bracket).
- These install into `<sysroot>/usr/lib/` and gcc's specs reference them by the standard names.

### 2.2 A standard 5-arg `__libc_start_main`
**This is the silicon-critical one.** gv3's entry is `void __libc_start_main(long *initial_sp)`
— ONE arg (`libc/src/start.c:15`, `libc/src/crt/crt0.S:30`). The standard ABI is:
```c
int __libc_start_main(int (*main)(int,char**,char**), int argc, char **argv,
                      void (*init)(void), void (*fini)(void), void (*rtld_fini)(void), void *stack_end);
```
A stock `crt1.o` calls the 5+-arg form; if gv3 keeps the 1-arg form, a stock crt1 passes a
garbage frame → **builds clean, crashes on the T113.** So Tier 2 must rewrite `__libc_start_main`
to the standard signature AND rewrite crt0/crt1 to marshal `argc/argv/envp` from the initial
stack into that signature — which changes the **crt ↔ custom-kernel entry handoff** (the kernel's
`kernel/elf.c` loader + `kernel/proc.c setup_user_stack` build the initial frame gv3's `_start`
reads). Re-validate on real hardware.

### 2.3 Build libgcc (the compiler runtime)
`-nostdlib` also drops **libgcc** — gcc's helper routines (`__aeabi_idiv`, 64-bit divide,
soft-float, unwind). gv3 gets away without it today ONLY because coreutils happens to emit no
such helper; a larger program (or `-mgeneral-regs-only` soft-float paths) would fail to link.
A real toolchain MUST provide libgcc:
- Simplest: build gcc's `libgcc.a` for the target and install it into the sysroot (gcc's own
  `libgcc` is libc-independent — it can be built once the compiler exists).
- Then gv3 links `-lgcc` implicitly via specs, like every real toolchain.

### 2.4 A proper sysroot layout
`libc/build.sh` installs into a sysroot instead of a loose `SUBSTRATE_DIR`:
```
<sysroot>/usr/include/   ← libc/include/* + staged UAPI (gv3_syscalls.h, gv3_abi.h) ONLY
                            (NOT libc/src — those are build-time-private headers)
<sysroot>/usr/lib/       ← libc.a, libc.so, crt1.o crti.o crtn.o, libgcc.a, ld-gv3.so.1
```

## 3. Tier 3 — building gcc + binutils for an `arm-gv3-linux-gnueabihf` triple

This is what makes musl a *musl* toolchain: gcc was **configured and compiled** to default to it.
The canonical method is a **two-pass (three-stage) bootstrap** — this is exactly what
crosstool-NG / Buildroot's internal toolchain do; consider using crosstool-NG rather than
hand-rolling.

### 3.1 Pick the triple
`arm-gv3-linux-gnueabihf` (or `armv7-gv3-eabihf`). The vendor field (`gv3`) is how gcc's config
selects our libc/spec defaults. It must be a triple gcc's `config.sub` accepts (custom vendor
strings are fine; `*-linux-gnueabihf` keeps the ARM hard-float Linux conventions we use).

### 3.2 binutils first
Build `binutils` (`as`, `ld`, `ar`, …) `--target=arm-gv3-linux-gnueabihf --prefix=<toolchain>`.
binutils is libc-independent, so this is the easy part. Bake gv3's defaults here where possible:
ld's default linker script / `--dynamic-linker=/lib/ld-gv3.so.1` can be set via an emulation
tweak or left to gcc specs (§3.5).

### 3.3 Bootstrap gcc (stage 1 — "gcc-first", C-only, no libc)
Build a minimal C compiler `--target=<triple> --without-headers --with-newlib --disable-shared
--enable-languages=c`. This stage-1 gcc can compile freestanding code (enough to build gv3libc)
but has no libc knowledge yet. This is the classic chicken-egg break: you need a compiler to
build the libc, but the final compiler needs the libc.

### 3.4 Build gv3libc (Tier 2 artifacts) with stage-1 gcc
Use stage-1 gcc to build the full Tier 2 sysroot (§2): crt suite, libc.a/.so, libgcc, headers,
ld-gv3.so.1. Install into `<sysroot>`.

### 3.5 Rebuild gcc (stage 2 — the real compiler, `--with-sysroot`)
Rebuild gcc `--with-sysroot=<sysroot> --enable-shared --enable-languages=c` now that the libc
exists. This stage-2 gcc has gv3 baked in as the default libc. The libc-specific defaults live
in gcc's **config** for the `gv3` vendor (a `gcc/config/arm/linux-gv3.h` or a `.specs` compiled
in): default crt names, default `--dynamic-linker=/lib/ld-gv3.so.1`, default include/lib search
under the sysroot. After this, `arm-gv3-linux-gnueabihf-gcc hello.c -o hello` Just Works.

### 3.6 The static-load-address question
gv3 static currently uses `-T user.ld` to pin ET_EXEC @ 0x10000 (below the custom kernel's
0x41000000 map). A real toolchain would bake this into ld's default script for the triple —
OR (better) determine whether it's even needed: 0x10000 is the standard ARM load address, so a
default-layout static ELF may already be correct, making `user.ld` redundant (its real content
is the `/DISCARD/` of `.ARM.attributes`/`.note`, which a spec can also set). Verify against the
kernel's ELF loader before deciding.

## 4. Integration with forge (what changes once the toolchain exists)

Once gv3 is a real toolchain, the forge side gets genuinely simpler:
- gv3 becomes a **`host-tarball-bin`-style host package** like the Bootlin toolchains: a
  `hostpackages/toolchain-gv3/recipe.sh` that provisions the built toolchain (or builds it via a
  new `host-crosstool` class). `LIBC=custom` selects `ROOTFS_CROSS_COMPILE=arm-gv3-linux-gnueabihf-`.
- **`ccprofile.sh`'s per-libc branch DELETES** — both libcs are now "a normal cross-link with the
  toolchain's own crt/libc"; only `_ARCH_FLAGS` + `-static`/dynamic remain, identical for both.
- **`libc/libc-profile.sh` DELETES** — its contents became gcc's compiled-in specs.
- `run-recipe.sh` loses the `SUBSTRATE_DIR`/`STAGE_INC`/`SUBSTRATE_CRT`/`SUBSTRATE_LIB` env
  plumbing (that existed only to hand-feed the blob).
- The rootfs runtime-`.so` staging (`ld-gv3.so.1` + `libc.so` into `/lib`) STILL exists — that's
  a runtime concern the toolchain doesn't touch. Unchanged.

So the payoff on the forge side is real (delete ccprofile branch + libc-profile.sh + substrate
env plumbing) — but it's *bought* by owning a gcc/binutils build, which is the dominant new cost.

## 5. Silicon traps (verify on real T113 + QEMU virt, not just "it links")
- **crt1 ↔ kernel entry**: the new 5-arg `__libc_start_main` + crt1 must marshal argc/argv/envp
  from the exact initial stack `kernel/proc.c setup_user_stack` builds. A mismatch = garbage
  `main` args, crashes at first arg use.
- **static load addr**: confirm ET_EXEC entry stays 0x00010000 with NO PT_INTERP (readelf -h/-l);
  a wrong load addr collides with the kernel map — links clean, faults on silicon.
- **dynamic interp**: confirm PT_INTERP == /lib/ld-gv3.so.1 and it's staged as a real file.
- **VFP / virt variant**: the toolchain must still support BOTH the t113 (VFP) and virt
  (`-mgeneral-regs-only`) arch tunes — a single prebuilt toolchain must not freeze one ISA.
  (This intersects the "virt as a board" work; a toolchain is arch-flag-parameterized at compile
  time, so keep arch on the CLI as today.)
- **libgcc soft-float**: once libgcc is linked, verify the `-mgeneral-regs-only` (virt) build
  pulls the soft-float helpers correctly — that's the case that fails TODAY under `-nostdlib`.

## 6. Recommendation for whoever picks this up
- If the goal is a **cleaner forge engine**: DON'T do this — use a `gv3-cc` wrapper instead
  (hides the irreducible flags behind a conforming `cc`; engine becomes uniform; ~1 day; no
  silicon risk). The toolchain graduation is disproportionate for that goal.
- If the goal is **"gv3libc should be a real, standalone libc port"** (a legitimate learning
  milestone in its own right): do **Tier 2 first** (crt suite + libgcc + 5-arg entry — this is
  the real libc-completeness work and is valuable independent of gcc), validate on hardware, THEN
  Tier 3 (the gcc/binutils bootstrap, ideally via crosstool-NG). Budget weeks and a hardware
  re-validation pass, not a refactor sprint.
