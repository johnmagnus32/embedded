Add a Debug Adapter Protocol (DAP) mode to sim-dbg so VS Code can use it directly without GDB. Work in `/home/johmagnu/learning/simple-stm32/dbg`. Read all files in `src/` before making changes. Build with `make` from `dbg/`.

## Problem

VS Code's debug UI requires a Debug Adapter Protocol (DAP) server. Currently the only way to debug from VS Code is via the cortex-debug extension + GDB, which requires GDB >= 9 and `gdb-multiarch` — not available on all systems.

sim-dbg already has all the debugging logic (DWARF symbols, breakpoints, stepping, variable inspection) but speaks a human-readable REPL on stdin/stdout. Adding a `--dap` flag that switches to DAP mode lets VS Code use sim-dbg directly.

## Goal

```bash
# Terminal: start emulator
sim/build/sim-core --machine gameboy --firmware gameboy.elf --gdb 1234

# VS Code launch.json points to sim-dbg as the debug adapter
# sim-dbg connects to the emulator's GDB RSP stub and speaks DAP to VS Code
```

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Debug on Simulator",
            "type": "sim-dbg",
            "request": "attach",
            "executable": "${workspaceFolder}/projects/gameboy/build/gameboy.elf",
            "gdbTarget": "localhost:1234"
        }
    ]
}
```

## DAP protocol overview

DAP is JSON over stdin/stdout with Content-Length headers (same framing as LSP):

```
Content-Length: 85\r\n
\r\n
{"seq":1,"type":"request","command":"initialize","arguments":{"adapterID":"sim-dbg"}}
```

Responses and events use the same framing. Each message has `seq`, `type` (request/response/event), and `command`.

## Part 1: DAP message framing

Add `src/dap.c` and `src/dap.h` for reading/writing DAP messages on stdin/stdout:

```c
/* Read one DAP message from stdin. Returns malloc'd JSON string. */
char *dap_read(void);

/* Write a DAP message to stdout. */
void dap_write(const char *json);
```

Implementation: read `Content-Length: N\r\n\r\n`, then read N bytes. Write with the same header.

## Part 2: Core DAP request handlers

Add `src/dap_server.c` — the main DAP loop. Handle these requests:

### `initialize`
Return capabilities:
```json
{"supportsConfigurationDoneRequest": true, "supportsFunctionBreakpoints": true}
```
Then send `initialized` event.

### `attach`
Parse `gdbTarget` and `executable` from arguments. Connect to the GDB RSP stub. Load ELF for symbols/DWARF.

### `configurationDone`
Send `stopped` event with reason `"entry"`.

### `setBreakpoints`
For each breakpoint in `arguments.breakpoints`:
1. Resolve `source.path` + `line` to address via DWARF (`resolve_breakpoint`)
2. Send `Z0,addr,2` to stub
3. Return verified breakpoints with resolved line numbers

### `continue`
Send `c` to stub. When stop reply arrives, send `stopped` event with reason `"breakpoint"` or `"pause"`.

### `next` (step over)
Same logic as the REPL `next` command. Send `stopped` event when done.

### `stepIn`
Same as REPL `step`. Send `stopped` event.

### `pause`
Send 0x03 (Ctrl+C) to stub. Send `stopped` event with reason `"pause"`.

### `stackTrace`
Read registers via `g` packet. Build frames from PC and LR using DWARF line info. Return:
```json
{"stackFrames": [{"id": 0, "name": "main", "source": {"path": "/path/to/main.c"}, "line": 42}]}
```

### `scopes`
Return one scope: `{"name": "Locals", "variablesReference": 1}`.

### `variables`
For the locals scope: use `vars_on_stack()` from elf_sym to find local variables at the current PC. For each variable, read its value via `m` packet and format with DWARF type info.

### `evaluate`
Reuse the existing `print` command logic — parse expression, resolve via DWARF, read memory, format result.

### `threads`
Return one thread (single-core Cortex-M):
```json
{"threads": [{"id": 1, "name": "main"}]}
```

### `disconnect`
Close the RSP connection. Exit.

## Part 3: Entry point changes

Update `src/main.c` to support `--dap` flag:

```c
if (dap_mode) {
    dap_server_run(connect_str, elf_path);  /* reads stdin, writes stdout */
} else {
    /* existing REPL */
}
```

When `--dap` is passed, sim-dbg reads DAP JSON from stdin and writes DAP JSON to stdout. No REPL, no readline.

## Part 4: VS Code extension manifest

Create `dbg/vscode-extension/package.json` — a minimal VS Code extension that registers the `sim-dbg` debug adapter:

```json
{
    "name": "sim-dbg",
    "displayName": "ARM Simulator Debugger",
    "version": "0.1.0",
    "engines": {"vscode": "^1.80.0"},
    "categories": ["Debuggers"],
    "contributes": {
        "debuggers": [{
            "type": "sim-dbg",
            "label": "ARM Simulator",
            "program": "${workspaceFolder}/dbg/build/sim-dbg",
            "args": ["--dap"],
            "configurationAttributes": {
                "attach": {
                    "required": ["executable", "gdbTarget"],
                    "properties": {
                        "executable": {"type": "string", "description": "Path to ELF firmware"},
                        "gdbTarget": {"type": "string", "description": "GDB RSP target (host:port)"}
                    }
                }
            }
        }]
    }
}
```

Install locally: `ln -s $(pwd)/dbg/vscode-extension ~/.vscode/extensions/sim-dbg`

## Part 5: Build system

Update `dbg/Makefile` — no new dependencies. `dap.c` and `dap_server.c` are just JSON formatting + the existing debug logic.

## Testing

1. Start sim-core: `sim/build/sim-core --machine gameboy --firmware gameboy.elf --gdb 1234`
2. Test DAP manually: `echo '...' | dbg/build/sim-dbg --dap --connect localhost:1234 --elf gameboy.elf`
3. Test from VS Code: install extension, press F5, verify breakpoints/stepping/variables work
4. Verify REPL mode still works: `dbg/build/sim-dbg --connect localhost:1234 --elf gameboy.elf`
5. Run sim tests: `cd sim && make test` — 38 tests still pass (DAP changes are in dbg/ only)
