Decouple the debugger from the emulator and the debugger UI from the simulation UI. The debugger lives in its own top-level directory `dbg/`, separate from `sim/mcu/`. Read all source files in `mcu-sim/mcu/src/` before making changes.

## Goal

Split into three components that mirror real embedded development:

```
simple-stm32/
├── sim/          ← emulator (sim-core + sim-web)
├── dbg/          ← debugger CLI (sim-dbg) — separate project
├── rtos/           ← the RTOS
├── projects/     ← firmware (gameboy)
├── tests/        ← integration/perf tests
└── docs/         ← design documents
```

```
Real hardware:
  Device running    → you see the LCD, hear audio, press buttons
  GDB via J-Link    → separate terminal with source, registers, breakpoints

Your emulator:
  sim-core (sim/)   → emulator, runs firmware, serves chardevs + debug stub
  sim-web  (sim/)   → simulation UI: display, audio, buttons, UART, trace timeline (keep as Python)
  sim-dbg  (dbg/)   → debugger CLI: source, registers, breakpoints, stepping, expressions
```

Each is a separate process. Any combination works:

```bash
# Just play the game — no debugger
sim-core --machine gameboy --firmware gameboy.elf --chardev display=9004 ...
sim-web --connect-chardevs  # shows display, audio, buttons

# Play + debug
sim-core --machine gameboy --firmware gameboy.elf --debug 9001 --chardev display=9004 ...
sim-web --connect-chardevs  # simulation UI
sim-dbg --connect localhost:9001 --elf gameboy.elf  # debugger CLI

# Debug only, no simulation UI (headless)
sim-core --machine gameboy --firmware gameboy.elf --debug 9001
sim-dbg --connect localhost:9001 --elf gameboy.elf
```

## Implementation order

Do this incrementally, each step independently testable:

1. **Part 1: Add debug stub to sim-core** alongside the existing debug server (both work during transition)
2. **Part 2: Strip debug features from sim-web** (mechanical refactor)
3. **Part 3: Build sim-dbg** as a new executable (biggest piece, can be done incrementally)

## Part 1: sim-core (emulator + debug stub)

sim-core:
- Loads firmware ELF segments (no symbols, no DWARF)
- Creates machine, runs CPU, simulates all hardware
- Serves chardevs: display (9004), UART (9002), trace (9003), audio (9005), IO (9006)
- Optionally serves a debug stub on `--debug <port>`
- The debug stub exposes raw state: registers, memory, step instruction, breakpoints by address
- Runs at full speed when no debugger is connected

### Debug stub protocol (JSON over newline TCP):

```
{"cmd":"halt"}                           → {"stopped":true,"pc":N,"cycles":N}
{"cmd":"continue"}                       → (unsolicited reply when stopped) {"stopped":true,"pc":N,"cycles":N}
{"cmd":"step"}                           → {"stopped":true,"pc":N,"cycles":N}
{"cmd":"stepi","n":N}                    → execute N instructions
{"cmd":"reset"}                          → {"ok":true}
{"cmd":"regs"}                           → {"regs":[r0..r15],"xpsr":N,"msp":N,"psp":N}
{"cmd":"mem","addr":N,"len":N}           → {"mem":"hexbytes"}
{"cmd":"writemem","addr":N,"data":"hex"} → {"ok":true}
{"cmd":"break","addr":N}                 → {"ok":true}
{"cmd":"delbreak","addr":N}              → {"ok":true}
{"cmd":"listbreak"}                      → {"breakpoints":[addr1,addr2,...]}
{"cmd":"status"}                         → {"running":true/false,"pc":N,"cycles":N}
```

### Breakpoints

Use BKPT instruction patching (0xBE00). Zero per-tick overhead. Modify `armv7m_cpu_step` to return `CPU_BREAKPOINT` when it hits a BKPT instruction.

On continue after a breakpoint hit: restore the original instruction, single-step one instruction (to execute it), re-patch BKPT, then resume.

### Async halt during continue

The stub sends `{"stopped":true,...}` as an **unsolicited response** when a breakpoint is hit during `continue` — the client doesn't poll for it. The client reads the response asynchronously.

For Ctrl+C halt: sim-dbg sends `{"cmd":"halt"}` on the same TCP connection while waiting for the stop response. The stub checks for incoming commands between ticks via non-blocking `select()` in the IO polling loop (every 10K ticks). When it sees `halt`, it stops the CPU and sends the stop response. This works because the stub's command socket is checked periodically during `continue`, not only when the CPU is stopped.

## Part 2: sim-web (simulation UI — keep as Python)

sim-web is the "hardware front panel." Keep it as Python — it's a proxy + HTTP/WebSocket server, and Python is fine for that. Only strip the debug-related features.

### What sim-web shows:
- LCD display canvas (from display chardev via WebSocket)
- Audio playback (from audio chardev via WebSocket)
- Button inputs (to IO chardev)
- Volume slider (to IO chardev)
- UART console (from UART chardev via WebSocket)
- Trace timeline (from trace chardev via WebSocket)

### What sim-web does NOT show:
- No source code panel
- No register view
- No memory map / RTOS task introspection
- No breakpoint controls
- No step/next/continue buttons
- No expression evaluator

Remove debug-related endpoints: `POST /cmd`, `GET /init`, `GET /ws-status`. Keep: `GET /`, `GET /ws-display`, `GET /ws-uart`, `GET /ws-trace`, `GET /ws-audio`, `POST /io`, `POST /gpio`, `GET /audio`.

sim-web no longer connects to the debug port. It only connects to chardevs.

Update index.html: remove source panel, debugger command input, register display, step/next/continue buttons, breakpoint gutter, memory map panel, expression evaluator. Keep: LCD canvas, UART console, trace timeline, button controls, volume slider, audio playback.

## Part 3: sim-dbg (debugger CLI)

A GDB-like command-line interface. Connects to sim-core's debug stub, loads the ELF for symbols/DWARF.

### Caveat: ELF symbol mismatch

sim-dbg loads its own copy of the ELF for symbols/DWARF. If the ELF doesn't match the firmware running in sim-core (e.g., different build, stripped binary), breakpoints will land at wrong addresses and variable inspection will return garbage. sim-dbg should warn if the ELF entry point doesn't match the stub's reported PC at reset.

### Architecture

```
dbg/
  src/
    dbg_main.c          ← entry point: parse args, connect, load ELF, REPL loop
    dbg_client.c/h      ← TCP client for the debug stub protocol
    dbg_cmd.c/h         ← command handlers (break, step, next, print, etc.)
    dbg_eval.c/h        ← expression evaluator (copied from sim/mcu/src/debug/)
    dbg_tasks.c/h       ← RTOS task introspection (copied from sim/mcu/src/debug/)
    elf_sym.c/h         ← ELF/DWARF parser (copied from sim/mcu/src/core/)
  Makefile
  build/
```

The debugger has no build-time dependency on `sim/`. It connects to sim-core at runtime via TCP. The shared code (`elf_sym.c`, `dbg_eval.c`, `dbg_tasks.c`) is copied into `dbg/src/` — not imported or symlinked. This keeps the two projects fully independent.

### Commands

```
(dbg) break main
Breakpoint 1 at 0x08001234: file src/main.c, line 42

(dbg) continue
Breakpoint 1, main() at src/main.c:42
42      uart = DEVICE_DT_GET(usart2);

(dbg) next
43      heap_init(&_heap_start, (size_t)&_heap_size);

(dbg) step
[entering heap_init() at lib/heap.c:10]

(dbg) finish
[returning to main() at src/main.c:44]

(dbg) print tasks[0].sp
$1 = 0x200003E0

(dbg) info regs
r0=0x00000000 r1=0x20000100 r2=0x00000003 ...

(dbg) info tasks
  ID  Name      Stack       State
  0   task_a    312/512     RUNNING
  1   task_b    128/512     SLEEPING

(dbg) backtrace
#0  main() at src/main.c:44
#1  reset_handler() at startup_m4.s:15

(dbg) list
37  void main(void)
38  {
39      uart = DEVICE_DT_GET(usart2);
40
41      heap_init(&_heap_start, (size_t)&_heap_size);
42 →    lcd_init();

(dbg) halt
[halted at 0x08002456]

(dbg) quit
Disconnected.
```

### How rich commands work over the stub

**`break main`:** Look up "main" in ELF symbol table → address. Send `{"cmd":"break","addr":N}`.

**`next` (source-level step over):**
1. Read PC via `{"cmd":"regs"}`
2. Look up DWARF line table → current source line
3. Read instruction at PC via `{"cmd":"mem","addr":PC,"len":4}` (read 4 bytes to handle 32-bit Thumb-2)
4. If it's a BL/BLX (function call):
   - Read LR or use DWARF CFI to find the return address for the current frame
   - Set temp breakpoint at the return address: `{"cmd":"break","addr":return_addr}`
   - Also set temp breakpoint at PC+4 (for the simple case where the call returns normally)
   - Send `{"cmd":"continue"}`
   - When stopped: remove temp breakpoints
   - Note: PC+4 alone doesn't handle tail calls, indirect calls, or exceptions. The return address breakpoint is the reliable fallback.
5. If not a call: `{"cmd":"step"}`, check if DWARF line changed, repeat until it does

**`finish` (run until current function returns):**
1. Read current frame's return address from DWARF CFI or by reading LR
2. Set temp breakpoint at return address
3. Continue

**`print tasks[0].sp`:** Resolve via DWARF → address. Read memory via `{"cmd":"mem",...}`. Format with type info. Each `->` dereference or array index is another `{"cmd":"mem",...}` round-trip.

**`info tasks`:** Read `num_tasks`, TCB array, stacks via multiple `{"cmd":"mem",...}` calls. Format table using DWARF type info for TCB layout.

### REPL implementation

Use `readline` for command history and line editing:

```c
#include <readline/readline.h>
#include <readline/history.h>

while (1) {
    char *line = readline("(dbg) ");
    if (!line) break;
    if (*line) add_history(line);
    dbg_handle_command(client, elf, line);
    free(line);
}
```

Link with `-lreadline`. Fall back to `fgets` if readline isn't available.

### Async stop notification during continue

When the user types `continue`, sim-dbg sends the command and waits for the stop response while also watching stdin for Ctrl+C:

```c
dbg_send(client, "{\"cmd\":\"continue\"}");
printf("[running...]\n");

// Set up signal handler for SIGINT (Ctrl+C)
volatile int got_sigint = 0;
signal(SIGINT, [](int) { got_sigint = 1; });

while (1) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(client->fd, &fds);
    struct timeval tv = {0, 100000};  // 100ms timeout to check sigint flag
    int ready = select(client->fd + 1, &fds, NULL, NULL, &tv);

    if (ready > 0 && FD_ISSET(client->fd, &fds)) {
        // Stub sent unsolicited stop reply (breakpoint hit)
        char *resp = dbg_recv(client);
        display_stop_info(resp);
        break;
    }
    if (got_sigint) {
        // User pressed Ctrl+C — send halt
        got_sigint = 0;
        dbg_send(client, "{\"cmd\":\"halt\"}");
        char *resp = dbg_recv(client);
        display_stop_info(resp);
        break;
    }
}

// Restore default SIGINT handler
signal(SIGINT, SIG_DFL);
```

## Part 4: Build system

### sim/mcu/Makefile (updated — remove debug command files, add stub)

```makefile
# sim-core: emulator + debug stub (no DWARF, no expression eval)
CORE_SRCS = src/main.c src/debug/dbg_stub.c src/debug/elf_load.c \
            src/core/*.c src/arch/armv7m/*.c src/hw/stm32/*.c \
            src/devices/*.c src/soc/*.c src/machine/*.c
```

Remove `dbg_cmd.c`, `dbg_eval.c`, `dbg_tasks.c`, `dbg_server.c`, `elf_sym.c` from sim's build. These move to `dbg/`.

`elf_load.c` is a new stripped-down ELF loader (program segments only, no `.symtab`/`.debug_*`).

### dbg/Makefile (new)

```makefile
CC = gcc
CFLAGS = -Wall -O2 -g
BUILD = build
SRC = src

SRCS = $(SRC)/dbg_main.c $(SRC)/dbg_client.c $(SRC)/dbg_cmd.c \
       $(SRC)/dbg_eval.c $(SRC)/dbg_tasks.c $(SRC)/elf_sym.c
OBJS = $(patsubst $(SRC)/%.c,$(BUILD)/%.o,$(SRCS))

all: $(BUILD)/sim-dbg

$(BUILD)/sim-dbg: $(OBJS)
	$(CC) -o $@ $^ -lreadline

$(BUILD)/%.o: $(SRC)/%.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(SRC) -c -o $@ $<

clean:
	rm -rf $(BUILD)

.PHONY: all clean
```

sim-web stays as Python in `mcu-sim/mcu/src/sim-web/` — no build step needed.

## Part 5: Launch scripts

```bash
# sim/mcu/sim — run the game (no debugger)
#!/bin/bash
DIR="$(cd "$(dirname "$0")" && pwd)"
"$DIR/build/sim-core" --machine gameboy --firmware "$1" \
    --chardev display=9004 --chardev usart2=9002 --chardev audio=9005 \
    --chardev trace=9003 --chardev io=9006 &
SIM_PID=$!
sleep 1
python3 "$DIR/src/sim-web/sim-web.py" \
    --chardev display=9004 --chardev usart2=9002 \
    --chardev audio=9005 --chardev trace=9003 --chardev io=9006 --port 3000
kill $SIM_PID

# sim/mcu/sim-debug — run the game + debugger
#!/bin/bash
DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$DIR/.." && pwd)"
"$DIR/build/sim-core" --machine gameboy --firmware "$1" --debug 9001 \
    --chardev display=9004 --chardev usart2=9002 --chardev audio=9005 \
    --chardev trace=9003 --chardev io=9006 &
SIM_PID=$!
sleep 1
python3 "$DIR/src/sim-web/sim-web.py" \
    --chardev display=9004 --chardev usart2=9002 \
    --chardev audio=9005 --chardev trace=9003 --chardev io=9006 --port 3000 &
WEB_PID=$!
sleep 1
"$ROOT/dbg/build/sim-dbg" --connect localhost:9001 --elf "$1"
# When debugger exits, clean up
kill $WEB_PID $SIM_PID 2>/dev/null
```

## Testing

1. `./sim gameboy.elf` — game runs in browser, no debugger. Display, audio, buttons, UART, trace all work.
2. `./sim-debug gameboy.elf` — game runs in browser AND debugger CLI appears in terminal. Can set breakpoints, step, inspect variables while the game is visible in the browser.
3. Start sim-core alone, then attach sim-dbg later — verify it halts and can inspect state.
4. Disconnect sim-dbg (quit) — verify sim-core resumes running at full speed.
5. Ctrl+C during `continue` — verify it halts promptly.
6. `next` over a function call — verify it stops at the next source line, not inside the called function.
7. `finish` — verify it runs until the current function returns.
8. Run perf tests: `./sim` should show higher MIPS than `./sim-debug` (no debug stub overhead when no debugger connected).
