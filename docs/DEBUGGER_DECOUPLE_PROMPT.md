Decouple the debugger from the emulator and the debugger UI from the simulation UI. Work in `/home/johmagnu/learning/simple-stm32/sim`. Read all source files in `src/` before making changes. Build with `make` from `sim/`.

## Goal

Split into three components that mirror real embedded development:

```
Real hardware:
  Device running    → you see the LCD, hear audio, press buttons
  GDB via J-Link    → separate terminal with source, registers, breakpoints

Your emulator:
  sim-core          → emulator, runs firmware, serves chardevs + debug stub
  sim-web           → simulation UI: display, audio, buttons, UART, trace timeline
  sim-dbg           → debugger CLI: source, registers, breakpoints, stepping, expressions
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

## Part 1: sim-core (emulator)

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
{"cmd":"continue"}                       → (reply when stopped) {"stopped":true,"pc":N,"cycles":N}
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

Breakpoints use BKPT instruction patching (0xBE00). Zero per-tick overhead. Modify `armv7m_cpu_step` to return `CPU_BREAKPOINT` when it hits a BKPT instruction.

## Part 2: sim-web (simulation UI)

sim-web is the "hardware front panel." It connects to sim-core's chardevs (not the debug stub) and serves a web UI.

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

Update index.html: remove source panel, debugger command input, register display, step/next/continue buttons, breakpoint gutter, memory map panel, expression evaluator. Keep: LCD canvas, UART console, trace timeline, button controls, volume slider, audio playback.

## Part 3: sim-dbg (debugger CLI)

A GDB-like command-line interface. Connects to sim-core's debug stub, loads the ELF for symbols/DWARF.

### Architecture

```
src/debugger/
  dbg_main.c          ← entry point: parse args, connect, load ELF, REPL loop
  dbg_client.c/h      ← TCP client for the debug stub protocol
  dbg_cmd.c/h         ← command handlers (break, step, next, print, etc.)
  dbg_eval.c/h        ← expression evaluator
  dbg_tasks.c/h       ← RTOS task introspection
  elf_sym.c/h         ← ELF/DWARF parser (copy from current debug/)
```

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

**`break main`:** Look up "main" in ELF → address. Send `{"cmd":"break","addr":N}`.

**`next`:** Read PC via `{"cmd":"regs"}`. Look up DWARF line. Read instruction via `{"cmd":"mem",...}`. If BL: set temp breakpoint at PC+4, continue. Else: step, check line, repeat.

**`print tasks[0].sp`:** Resolve via DWARF → address. Read memory via `{"cmd":"mem",...}`. Format with type info.

**`info tasks`:** Read `num_tasks`, TCB array, stacks via multiple `{"cmd":"mem",...}` calls. Format table.

### REPL

Use `readline` for command history. Use `select()` on stdin + stub socket to handle Ctrl+C during `continue`.

## Part 4: Build system

```makefile
all: $(BUILD)/sim-core $(BUILD)/sim-web $(BUILD)/sim-dbg

# sim-core: emulator + debug stub (no DWARF)
CORE_SRCS = src/main.c src/debug/dbg_stub.c src/debug/elf_load.c \
            src/core/*.c src/arch/armv7m/*.c src/hw/stm32/*.c \
            src/devices/*.c src/soc/*.c src/machine/*.c

# sim-web: simulation UI (chardev proxy, no debug)
WEB_SRCS = src/sim-web/sim-web.c

# sim-dbg: debugger CLI (DWARF, expressions, RTOS tasks)
DBG_SRCS = src/debugger/dbg_main.c src/debugger/dbg_client.c \
           src/debugger/dbg_cmd.c src/debugger/dbg_eval.c \
           src/debugger/dbg_tasks.c src/debugger/elf_sym.c
```

`elf_load.c` is a stripped-down ELF loader (segments only, no symbols/DWARF). sim-dbg uses the full `elf_sym.c`.

## Part 5: Launch scripts

```bash
# sim/sim — run the game (no debugger)
"$DIR/build/sim-core" --machine gameboy --firmware "$1" [chardevs...] &
"$DIR/build/sim-web" [chardev connections...] --port 3000

# sim/sim-debug — run the game + debugger
"$DIR/build/sim-core" --machine gameboy --firmware "$1" --debug 9001 [chardevs...] &
"$DIR/build/sim-web" [chardev connections...] --port 3000 &
"$DIR/build/sim-dbg" --connect localhost:9001 --elf "$1"
```

## Testing

1. `./sim gameboy.elf` — game runs in browser, no debugger
2. `./sim-debug gameboy.elf` — game in browser + debugger CLI in terminal
3. Attach sim-dbg to already-running sim-core — verify it halts and can inspect
4. Disconnect sim-dbg — verify sim-core resumes
5. `./sim` should show higher MIPS than `./sim-debug`
