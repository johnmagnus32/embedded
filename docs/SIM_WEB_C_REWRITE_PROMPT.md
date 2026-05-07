Rewrite sim-web from Python to C for better performance. Keep the Python version as `sim/mcu/sim` and add the C version as `sim/mcu/sim-fast` so both can be compared. Work in `/home/johmagnu/learning/simple-stm32/sim/mcu`. Read `src/sim-web/sim-web.py` and `src/sim-web/index.html` thoroughly before starting. Build with `make` from `sim/mcu/`.

## Problem

Python sim-web.py is a bottleneck — its single-threaded HTTP/WebSocket server can't drain chardev sockets fast enough, causing sim-core to block on TCP writes.

## Architecture

Create `src/sim-web/sim-web.c`. Single-threaded event loop using `select()` — no threads needed.

### What sim-web does:
1. Spawns sim-core as subprocess
2. Connects to 6 chardev TCP ports (debug, UART, trace, display, audio, IO)
3. Serves HTTP: `GET /` (index.html), `POST /cmd`, `POST /io`, `POST /gpio`, `GET /audio`
4. Serves WebSocket: `/ws-display` (delta-compressed frames), `/ws-uart`, `/ws-trace`, `/ws-status`, `/ws-audio`

### Event loop:
```c
while (1) {
    select() on: http_srv, sim_debug, sim_uart, sim_trace, sim_display, sim_audio, ws_clients[]
    // For each ready fd: read data, dispatch to handler
}
```

### Key components:
- Minimal HTTP parser (only the specific routes above)
- WebSocket RFC 6455: handshake (SHA-1 + base64), binary frame encode/decode
- Display delta compression (same algorithm as Python version)
- Display frame parsing (4-byte header + RGB565 pixels)
- Audio ring buffer (64KB cap)
- Trace event parsing (B:/E/I:/H:/F: prefixes)
- Debug command proxying (POST /cmd → debug socket → response)
- Embedded index.html via `xxd -i` or read from disk

### SHA-1 for WebSocket:
Implement inline (~80 lines) to avoid OpenSSL dependency.

## Build

```makefile
all: $(BUILD)/sim-core $(BUILD)/sim-web
$(BUILD)/sim-web: $(WEBOBJS)
	$(CC) -o $@ $^
```

## Launch scripts

- `sim/mcu/sim` — keep as-is, launches Python sim-web.py (unchanged)
- `sim/mcu/sim-fast` — new, launches C sim-web

## Testing

Run perf tests against both and write comparison to `tests/results/sim_web_comparison.txt`:

```
                    Python sim-web    C sim-web
MIPS (full stack):
Display FPS:
Button latency avg:
Audio underruns:
```

Verify identical behavior: display, audio, buttons, UART, trace, debug commands.
