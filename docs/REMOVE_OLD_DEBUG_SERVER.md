Remove the old integrated debug server from sim-core. The debug stub (`dbg_stub.c`) and the external debugger (`dbg/`) are the only debug path now. Work in `/home/johmagnu/learning/simple-stm32/sim`. Read all files in `src/debug/` before making changes. Build with `make` from `sim/`.

## What to delete

Remove these files from `sim/src/debug/`:
- `dbg_server.c` / `dbg_server.h` — the old TCP debug server that handled rich commands inline
- `dbg_cmd.c` / `dbg_cmd.h` — the old command dispatch (step, next, continue, print, memmap, etc.)
- `dbg_eval.c` / `dbg_eval.h` — the old expression evaluator
- `dbg_tasks.c` / `dbg_tasks.h` — the old RTOS task introspection

These are all superseded by `dbg/` which connects externally via the debug stub protocol.

## What stays in sim/src/debug/

- `dbg_stub.c` / `dbg_stub.h` — the thin debug stub (raw registers, memory, breakpoints by address, BKPT patching)
- `elf_load.c` / `elf_load.h` — the stripped-down ELF loader (program segments only, no symbols/DWARF)

## What to delete from sim/src/core/

- `elf_sym.c` / `elf_sym.h` — the full ELF/DWARF parser. This is only needed by the debugger, which now lives in `dbg/` and has its own copy.

## Update sim/Makefile

Remove the deleted files from SRCS:

```makefile
# Remove these:
#   src/debug/dbg_server.c src/debug/dbg_cmd.c src/debug/dbg_eval.c src/debug/dbg_tasks.c
#   src/core/elf_sym.c

# Keep these:
#   src/debug/dbg_stub.c src/debug/elf_load.c
```

## Update any includes

Search all remaining `.c` files in `sim/src/` for:
- `#include "dbg_server.h"` — remove
- `#include "dbg_cmd.h"` — remove
- `#include "dbg_eval.h"` — remove
- `#include "dbg_tasks.h"` — remove
- `#include "elf_sym.h"` — replace with `#include "elf_load.h"` where needed, or remove if not needed

`main.c` should only include `dbg_stub.h` and `elf_load.h` for debug-related functionality.

## Verify dbg/ has its own copies

Before deleting, confirm that `dbg/src/` already contains copies of:
- `elf_sym.c` / `elf_sym.h` (full DWARF parser)
- `dbg_eval.c` / `dbg_eval.h` (expression evaluator)
- `dbg_tasks.c` / `dbg_tasks.h` (RTOS task introspection)
- `dbg_cmd.c` / `dbg_cmd.h` (rich command handlers)

If `dbg/` doesn't exist yet, create it first (see `docs/DEBUGGER_DECOUPLE_PROMPT.md`) before deleting from sim.

## After deletion, sim/src/debug/ should contain only:

```
sim/src/debug/
  dbg_stub.c      (~250 lines — thin TCP stub, BKPT patching, raw register/memory access)
  dbg_stub.h
  elf_load.c       (~80 lines — load ELF program segments into flash/RAM, no symbols)
  elf_load.h
```

Down from the previous ~1500 lines across 6 files.

## Testing

1. `make` in `sim/` — verify it builds without the deleted files
2. `./sim ../projects/gameboy/build/gameboy.elf` — game runs without debugger
3. `sim-core --debug 9001` + `dbg/build/sim-dbg --connect localhost:9001` — debugger still works via the stub
4. Run `sim/tests/run_tests.py` — all semihosting tests pass (they don't use the debug server)
5. `--bench` mode still works
