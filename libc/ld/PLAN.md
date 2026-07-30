# ld-gv3 — a from-scratch dynamic linker (design + staged plan)

The runtime component that makes `make LINK=dynamic` binaries actually load: it
maps the shared `libc.so`, resolves symbols, and fills the GOT/PLT so calls like
`printf` reach their real addresses. Named `/lib/ld-gv3.so.1` (the PT_INTERP the
programs record).

## The contract (measured from our own dynamic binaries)

`readelf` on `build/bin/*` + `build/libc.so` says we must handle exactly:

- **Relocation types:** `R_ARM_JUMP_SLOT` (PLT slots), `R_ARM_GLOB_DAT` (GOT data
  words), and `R_ARM_RELATIVE` (base-relative fixups in PIC/PIE). That's it —
  a small, tractable set.
- **`.dynamic` tags:** STRTAB/SYMTAB/SYMENT (symbol + string tables), HASH or
  GNU_HASH (symbol lookup), JMPREL/PLTRELSZ/PLTREL/PLTGOT (PLT relocations),
  NEEDED (dependencies), STRSZ.

## How dynamic startup differs from static

```
  STATIC:  kernel -> _start(crt0) -> __libc_start_main -> main
  DYNAMIC: kernel loads the program AND the interpreter (PT_INTERP),
           jumps to the INTERPRETER's entry, NOT the program's.
           ld-gv3 then: maps libc.so, applies relocations, and finally
           jumps to the PROGRAM's real entry (_start), which proceeds as before.
```

So the linker is itself a program with an entry point, launched by the loader
(kernel or qemu) with the SAME initial stack (argc/argv/envp/auxv). It reads the
**auxv** to find the program it must finish loading:
`AT_PHDR`/`AT_PHNUM`/`AT_PHENT` (the program's program headers), `AT_ENTRY` (where
to jump when done), `AT_BASE` (the linker's own load address).

## The self-reference problem (the hard part)

`ld-gv3` may itself be PIC, so its OWN globals/calls need relocating before it can
run — but it can't call through an unrelocated GOT to do that. So its bootstrap
(`_dl_start`) must be written **self-contained**: no PLT calls, no unrelocated
global reads, until it has applied its own `R_ARM_RELATIVE` relocs. Only then can
it use normal C. This is stage S2 below and is where linkers are subtle.

## Staged plan (each stage independently verifiable)

| Stage | Adds | Verified by | Status |
|-------|------|-------------|--------|
| **L2** | reloc + symbol-resolution LOGIC as pure functions | host unit test, hand-built fixtures (`ld/test/test_reloc.c`) | ✅ done |
| **S1** | ELF/`.dynamic` parsing: find STRTAB/SYMTAB/relocs from a mapped image | host test: parse our REAL `libc.so`, resolve its symbols (`test_parse.c`) | ✅ done |
| **S2** | `_dl_start` bootstrap: self-relocate (R_ARM_RELATIVE) with no PLT | boots far enough to print via raw `write` syscall | next |
| **S3** | map `libc.so` (open+mmap the .so segments), apply its relocs | reference harness: our dynamic rootfs prints something | |
| **S4** | resolve program's JUMP_SLOT/GLOB_DAT against libc, jump to AT_ENTRY | `./test/dynamic.sh --gv3` passes on the mainline reference kernel | |
| **S5** | (later) kernel support: PT_INTERP + file-backed mmap in kernel/elf.c | our dynamic rootfs boots on OUR kernel too | |

**Host test suite:** `ld/test/run.sh` runs L2 + S1 (ASan-on). Both green. The S1
test lays out libc.so's LOAD segments in the low 4GB (MAP_32BIT) so the identical
32-bit-pointer parser runs on the host without truncation — same code as the target.

**Bugs the host tests already caught** (the payoff of testing pure/parse logic on
the host before the ARM bootstrap): (1) 32-bit `Elf32_Addr` truncating 64-bit host
pointers → drove the split of pure arithmetic (`reloc_value`) from the memory poke
(`reloc_apply`); (2) indexing `.dynamic` by file offset vs. `p_vaddr` → drove proper
per-LOAD-segment layout in the test, matching what S3 will do on the target.

## Testing (see rootfs/test/dynamic.sh)

Develop against the MAINLINE reference kernel first (proven known-good loader) —
a failure is then unambiguously ours. Only after S4 passes there do we add S5
(kernel support) and retest on our own kernel. qemu-arm user-mode (faster) isn't
packaged on this host, so the reference-kernel harness is the loop.

## Scope cuts (deliberate, for now)

- Lazy binding: NO — resolve everything eagerly at startup (BIND_NOW). Simpler;
  our binaries are tiny. (Lazy PLT resolution via a trampoline is a later refinement.)
- Multiple/transitive `.so` deps: only `libc.so` (one NEEDED) for now.
- Symbol versioning, TLS, `dlopen`: out of scope.
- GNU_HASH: parse HASH (SysV) first; add GNU_HASH only if needed.
