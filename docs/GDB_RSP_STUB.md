Replace the JSON debug stub protocol with GDB Remote Serial Protocol (RSP) so that standard GDB and VS Code can connect directly to sim-core. Work in `/home/johmagnu/learning/simple-stm32/sim`. Read `src/debug/dbg_stub.c` before making changes. Build with `make` from `sim/`.

## Goal

Replace `--debug <port>` (JSON protocol) with `--gdb <port>` (GDB RSP). After this change:

```bash
# Terminal 1: start emulator with GDB stub
sim-core --machine gameboy --firmware gameboy.elf --gdb 1234 --chardev display=9004 ...

# Terminal 2: connect GDB
arm-none-eabi-gdb gameboy.elf
(gdb) target remote :1234
(gdb) break main
(gdb) continue
(gdb) next
(gdb) print tasks[0].sp
(gdb) info registers
```

Also works with VS Code via the `cortex-debug` extension — zero custom extension code needed.

## GDB RSP protocol overview

Each packet: `$data#XX` where XX is the 2-digit hex checksum (sum of all bytes in data, mod 256). The receiver sends `+` to acknowledge or `-` to request retransmit.

## Part 1: RSP packet layer

Replace the JSON send/recv in `dbg_stub.c` with RSP packet handling:

```c
/* Send a GDB RSP packet */
static void gdb_send(int fd, const char *data)
{
    uint8_t sum = 0;
    for (const char *p = data; *p; p++) sum += (uint8_t)*p;
    char buf[16384];
    int n = snprintf(buf, sizeof(buf), "$%s#%02x", data, sum);
    write(fd, buf, n);
}

/* Receive a GDB RSP packet. Returns pointer to data (between $ and #).
 * Sends + acknowledgment. */
static int gdb_recv(int fd, char *buf, int bufsize)
{
    /* Read until we get $...#XX */
    int len = 0;
    char c;

    /* Skip until $ */
    do { if (read(fd, &c, 1) <= 0) return -1; } while (c != '$');

    /* Read until # */
    while (len < bufsize - 1) {
        if (read(fd, &c, 1) <= 0) return -1;
        if (c == '#') break;
        buf[len++] = c;
    }
    buf[len] = '\0';

    /* Read 2-byte checksum (we don't validate, just consume) */
    char ck[2];
    read(fd, ck, 2);

    /* Send ACK */
    write(fd, "+", 1);

    return len;
}
```

## Part 2: Packet handlers

Implement these GDB RSP packets:

### `?` — Stop reason query
GDB sends this first to ask why the target is stopped.

```c
case '?':
    gdb_send(fd, "S05");  /* SIGTRAP — stopped at breakpoint/reset */
    break;
```

### `g` — Read all registers
GDB expects registers in the ARM target order: r0-r12, sp, lr, pc, then xPSR (register 25 in GDB's ARM numbering). Each register is 4 bytes, little-endian hex (8 hex chars).

```c
case 'g': {
    char buf[256];
    int n = 0;
    /* r0-r12, sp, lr, pc */
    for (int i = 0; i < 16; i++) {
        uint32_t v = ctx->cpu->r[i];
        n += sprintf(buf + n, "%02x%02x%02x%02x",
                     v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF, (v >> 24) & 0xFF);
    }
    /* Registers 16-24: FPA registers (8 × 12 bytes) — send zeros */
    for (int i = 0; i < 8 * 12; i++)
        n += sprintf(buf + n, "00");
    /* Register 25: xPSR (CPSR in GDB's view) */
    uint32_t v = ctx->cpu->xpsr;
    n += sprintf(buf + n, "%02x%02x%02x%02x",
                 v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF, (v >> 24) & 0xFF);
    gdb_send(fd, buf);
    break;
}
```

Note: GDB's ARM register layout includes FPA floating point registers between the GPRs and CPSR. Most Cortex-M targets don't have FPA, but GDB expects them. Send zeros. The exact layout depends on the GDB target description — if GDB complains, you may need to send a target description XML via `qXfer:features:read` (see optional packets below).

### `G<hex>` — Write all registers
Parse the hex string back into registers. Same format as `g` response.

```c
case 'G': {
    const char *p = pkt + 1;
    for (int i = 0; i < 16; i++) {
        uint32_t v = 0;
        for (int b = 0; b < 4; b++) {
            unsigned int byte;
            sscanf(p, "%2x", &byte);
            v |= byte << (b * 8);
            p += 2;
        }
        ctx->cpu->r[i] = v;
    }
    /* Skip FPA registers */
    p += 8 * 12 * 2;
    /* xPSR */
    uint32_t v = 0;
    for (int b = 0; b < 4; b++) {
        unsigned int byte;
        sscanf(p, "%2x", &byte);
        v |= byte << (b * 8);
        p += 2;
    }
    ctx->cpu->xpsr = v;
    gdb_send(fd, "OK");
    break;
}
```

### `m<addr>,<length>` — Read memory

```c
case 'm': {
    uint32_t addr; int len;
    sscanf(pkt + 1, "%x,%x", &addr, &len);
    if (len > 4096) len = 4096;
    char buf[8200];
    int n = 0;
    for (int i = 0; i < len; i++)
        n += sprintf(buf + n, "%02x", membus_read8(ctx->bus, addr + i));
    gdb_send(fd, buf);
    break;
}
```

### `M<addr>,<length>:<hex>` — Write memory

```c
case 'M': {
    uint32_t addr; int len;
    sscanf(pkt + 1, "%x,%x", &addr, &len);
    const char *hex = strchr(pkt, ':');
    if (!hex) { gdb_send(fd, "E01"); break; }
    hex++;
    for (int i = 0; i < len; i++) {
        unsigned int b;
        sscanf(hex + i * 2, "%2x", &b);
        membus_write8(ctx->bus, addr + i, (uint8_t)b);
    }
    gdb_send(fd, "OK");
    break;
}
```

### `s` — Single step

```c
case 's':
    single_step(ctx);  /* reuse existing BKPT-aware step */
    gdb_send(fd, "S05");
    break;
```

### `c` — Continue

```c
case 'c': {
    int reason = run_continue(fd, ctx);
    if (reason == 2) {
        /* semihost exit */
        gdb_send(fd, "W00");  /* process exited with code 0 */
        return;
    }
    if (reason == 1) {
        /* halt requested (Ctrl+C from GDB sends 0x03 byte) */
        char tmp[256];
        read(fd, tmp, sizeof(tmp));  /* consume any pending data */
    }
    gdb_send(fd, "S05");  /* stopped */
    break;
}
```

### `Z0,<addr>,<kind>` — Set software breakpoint

```c
case 'Z':
    if (pkt[1] == '0') {
        uint32_t addr;
        sscanf(pkt + 3, "%x", &addr);
        /* Reuse existing BKPT patching */
        if (bp_find(addr) < 0 && nbp < MAX_BP) {
            bps[nbp].addr = addr;
            bp_patch(*ctx->flash, nbp);
            nbp++;
        }
        gdb_send(fd, "OK");
    } else {
        gdb_send(fd, "");  /* unsupported breakpoint type */
    }
    break;
```

### `z0,<addr>,<kind>` — Remove software breakpoint

```c
case 'z':
    if (pkt[1] == '0') {
        uint32_t addr;
        sscanf(pkt + 3, "%x", &addr);
        int idx = bp_find(addr);
        if (idx >= 0) {
            if (bps[idx].active) bp_unpatch(*ctx->flash, idx);
            bps[idx] = bps[--nbp];
        }
        gdb_send(fd, "OK");
    } else {
        gdb_send(fd, "");
    }
    break;
```

### `qSupported` — Capability query

```c
if (strncmp(pkt, "qSupported", 10) == 0) {
    gdb_send(fd, "PacketSize=4096;swbreak+");
    break;
}
```

`swbreak+` tells GDB we support software breakpoints.

### Other `q` queries — return empty (unsupported)

```c
if (pkt[0] == 'q') {
    gdb_send(fd, "");
    break;
}
```

## Part 3: Handle Ctrl+C (async interrupt)

When GDB wants to halt a running target, it sends a raw `0x03` byte (not a packet). Detect this in `run_continue`:

```c
static int run_continue(int fd, struct stub_ctx *ctx)
{
    int bp_at_pc = bp_find(ctx->cpu->r[REG_PC]);
    if (bp_at_pc >= 0) single_step(ctx);

    int tick_count = 0;
    while (1) {
        int r = ctx->mach->tick(ctx->board);
        if (r & CPU_SEMIHOST_EXIT) return 2;
        if (r & CPU_BREAKPOINT) return 0;

        if (++tick_count >= 10000) {
            tick_count = 0;
            perf_report(ctx);
            /* Check for Ctrl+C (0x03 byte) */
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(fd, &fds);
            struct timeval tv = {0, 0};
            if (select(fd + 1, &fds, NULL, NULL, &tv) > 0) {
                char c;
                read(fd, &c, 1);
                if (c == 0x03) return 1;  /* Ctrl+C halt */
            }
        }
    }
}
```

## Part 4: Update main.c

Replace `--debug` with `--gdb`:

```c
} else if (strcmp(argv[i], "--gdb") == 0 && i + 1 < argc) {
    gdb_port = atoi(argv[++i]);
}
```

```c
if (gdb_port > 0) {
    struct stub_ctx ctx = { ... };
    gdb_stub_run(&ctx, gdb_port);  /* renamed from dbg_stub_run */
}
```

## Part 5: Remove the JSON protocol

Delete all JSON-related code from `dbg_stub.c`:
- `send_resp`, `send_stop` (JSON formatting)
- `json_str`, `json_int` (JSON parsing)
- `dispatch` function (JSON command routing)
- The JSON command handlers (`cmd_regs`, `cmd_mem`, etc.)

Replace with the RSP packet handlers above. The breakpoint table, BKPT patching, `single_step`, and `run_continue` logic stay — only the protocol layer changes.

## Part 6: Update dbg/ to use GDB RSP

The external debugger (`dbg/`) should also switch from the JSON protocol to GDB RSP. Update `dbg/src/dbg_client.c` to send/receive RSP packets instead of JSON. The command logic in `dbg_cmd.c` stays the same — it just calls different client functions.

Alternatively, `sim-dbg` could use GDB's MI (Machine Interface) protocol to drive `arm-none-eabi-gdb` as a subprocess, which then connects to the RSP stub. This avoids reimplementing RSP parsing in the debugger and gives you all of GDB's features for free. But that adds a GDB dependency.

## Part 7: VS Code integration

Create `.vscode/launch.json` in the project root:

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Debug on Simulator",
            "type": "cortex-debug",
            "request": "launch",
            "servertype": "external",
            "gdbPath": "arm-none-eabi-gdb",
            "gdbTarget": "localhost:1234",
            "executable": "${workspaceFolder}/projects/gameboy/build/gameboy.elf",
            "runToEntryPoint": "main",
            "device": "STM32F411RE"
        }
    ]
}
```

Requires the `cortex-debug` VS Code extension (marus25.cortex-debug). Start sim-core with `--gdb 1234` first, then press F5 in VS Code.

## Testing

1. Start sim-core: `./build/sim-core --machine gameboy --firmware gameboy.elf --gdb 1234`
2. Connect GDB: `arm-none-eabi-gdb gameboy.elf -ex "target remote :1234"`
3. Test: `break main` → `continue` → should stop at main
4. Test: `info registers` → should show r0-r15, xPSR
5. Test: `x/16x 0x20000000` → should show RAM contents
6. Test: `next` → should step one source line
7. Test: `print variable_name` → should show value (GDB does the DWARF work)
8. Test: Ctrl+C during continue → should halt
9. Test: VS Code with cortex-debug → breakpoints, stepping, variable hover all work
10. Run sim tests: `python3 tests/run_tests.py` → semihosting tests still pass (they don't use the debug port)
