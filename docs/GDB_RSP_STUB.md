Replace the JSON debug stub protocol with GDB Remote Serial Protocol (RSP) so that standard GDB and VS Code can connect directly to sim-core. Work in `/home/johmagnu/learning/simple-stm32/sim/mcu`. Read `src/debug/dbg_stub.c` before making changes. Build with `make` from `sim/mcu/`.

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

## Part 3: Handle Ctrl+C and async packets during continue

When GDB wants to halt a running target, it sends a raw `0x03` byte (not a packet). But GDB can also send real packets while the target is running — most commonly `Z0`/`z0` to set/remove breakpoints without stopping. GDB expects `OK` back and the target keeps running.

Detect and handle both in `run_continue`:

```c
if (select(fd + 1, &fds, NULL, NULL, &tv) > 0) {
    char c;
    if (recv(fd, &c, 1, MSG_PEEK) > 0 && c == 0x03) {
        read(fd, &c, 1);  /* consume the 0x03 */
        return 1;  /* Ctrl+C halt */
    }
    /* Real packet — parse and handle without stopping */
    char pkt[256];
    if (gdb_recv(fd, pkt, sizeof(pkt)) > 0) {
        if (pkt[0] == 'Z' || pkt[0] == 'z') {
            dispatch(fd, ctx, pkt);  /* set/remove breakpoint, sends OK */
            /* Don't return — keep running */
        }
    }
}
```

This is required because GDB sets breakpoints while the target is running. When you type `break main` in GDB during execution, GDB sends `Z0,addr,2` and expects `OK` without the target stopping. The stub patches the BKPT instruction immediately and the target only stops when it actually reaches that address.

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

Update `dbg/src/dbg_client.c` to speak GDB RSP instead of the JSON protocol. The client sends RSP packets and parses RSP responses.

```c
/* dbg_client.c — GDB RSP client */

void dbg_send_packet(struct dbg_client *c, const char *data)
{
    uint8_t sum = 0;
    for (const char *p = data; *p; p++) sum += (uint8_t)*p;
    char buf[16384];
    int n = snprintf(buf, sizeof(buf), "$%s#%02x", data, sum);
    write(c->fd, buf, n);
    /* Wait for + ACK */
    char ack;
    read(c->fd, &ack, 1);
}

int dbg_recv_packet(struct dbg_client *c, char *buf, int bufsize)
{
    char ch;
    do { if (read(c->fd, &ch, 1) <= 0) return -1; } while (ch != '$');
    int len = 0;
    while (len < bufsize - 1) {
        if (read(c->fd, &ch, 1) <= 0) return -1;
        if (ch == '#') break;
        buf[len++] = ch;
    }
    buf[len] = '\0';
    char ck[2]; read(c->fd, ck, 2);  /* consume checksum */
    write(c->fd, "+", 1);  /* send ACK */
    return len;
}
```

Update the convenience wrappers to format RSP instead of JSON:

```c
void dbg_read_regs(struct dbg_client *c, uint32_t regs[16], uint32_t *xpsr)
{
    dbg_send_packet(c, "g");
    char buf[512];
    dbg_recv_packet(c, buf, sizeof(buf));
    /* Parse little-endian hex: each register is 8 hex chars */
    for (int i = 0; i < 16; i++) {
        uint32_t v = 0;
        for (int b = 0; b < 4; b++) {
            unsigned int byte;
            sscanf(buf + i * 8 + b * 2, "%2x", &byte);
            v |= byte << (b * 8);
        }
        regs[i] = v;
    }
    /* Skip FPA registers (8 × 12 bytes = 192 hex chars) */
    int xpsr_off = 16 * 8 + 192;
    uint32_t v = 0;
    for (int b = 0; b < 4; b++) {
        unsigned int byte;
        sscanf(buf + xpsr_off + b * 2, "%2x", &byte);
        v |= byte << (b * 8);
    }
    *xpsr = v;
}

void dbg_read_mem(struct dbg_client *c, uint32_t addr, uint8_t *out, int len)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "m%x,%x", addr, len);
    dbg_send_packet(c, cmd);
    char buf[8200];
    dbg_recv_packet(c, buf, sizeof(buf));
    for (int i = 0; i < len; i++) {
        unsigned int b;
        sscanf(buf + i * 2, "%2x", &b);
        out[i] = (uint8_t)b;
    }
}

void dbg_set_breakpoint(struct dbg_client *c, uint32_t addr)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "Z0,%x,2", addr);
    dbg_send_packet(c, cmd);
    char buf[64];
    dbg_recv_packet(c, buf, sizeof(buf));  /* expect "OK" */
}

void dbg_del_breakpoint(struct dbg_client *c, uint32_t addr)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "z0,%x,2", addr);
    dbg_send_packet(c, cmd);
    char buf[64];
    dbg_recv_packet(c, buf, sizeof(buf));
}

void dbg_continue(struct dbg_client *c)
{
    dbg_send_packet(c, "c");
    /* Response comes when target stops — handled by the async wait loop */
}

void dbg_step_instruction(struct dbg_client *c)
{
    dbg_send_packet(c, "s");
    char buf[64];
    dbg_recv_packet(c, buf, sizeof(buf));  /* "S05" */
}

void dbg_halt(struct dbg_client *c)
{
    /* Send raw 0x03 byte (Ctrl+C) — not a packet */
    char ctrl_c = 0x03;
    write(c->fd, &ctrl_c, 1);
}

uint32_t dbg_read_pc(struct dbg_client *c)
{
    uint32_t regs[16]; uint32_t xpsr;
    dbg_read_regs(c, regs, &xpsr);
    return regs[REG_PC];
}
```

The command handlers in `dbg_cmd.c` don't change — they call `dbg_read_regs`, `dbg_read_mem`, `dbg_set_breakpoint`, etc. which now speak RSP internally. The `(dbg)` CLI experience is identical.

This means sim-dbg can connect to any GDB RSP server — not just sim-core. It could connect to OpenOCD talking to real hardware, or to QEMU's GDB stub. And conversely, GDB can connect to sim-core's stub. Everything is interchangeable.

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
