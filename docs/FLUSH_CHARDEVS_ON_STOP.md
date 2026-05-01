Flush chardevs on every debug stop event in `sim/src/debug/dbg_stub.c`. Read the file before making changes. Build with `make` from `sim/`.

## Problem

Chardevs are only flushed on semihosting exit. When stepping through code that writes to UART or draws to the display, the output doesn't appear in the browser until the next `continue` (where `gameboy_tick`'s periodic flush runs). This means stepping over `uart_print("hello")` doesn't show "hello" in the UART console until you resume.

## Fix

Add `chardev_flush_all(ctx->chardevs)` to `send_stop()` so output is visible immediately after every breakpoint hit, step, and halt:

```c
static void send_stop(int fd, struct stub_ctx *ctx)
{
    if (ctx->chardevs)
        chardev_flush_all(ctx->chardevs);
    char buf[128];
    snprintf(buf, sizeof(buf),
        "{\"stopped\":true,\"pc\":%u,\"cycles\":%lu}",
        ctx->cpu->r[REG_PC], (unsigned long)ctx->cpu->cycle_count);
    send_resp(fd, buf);
}
```

## Testing

1. Start sim-core with `--debug` and chardevs
2. Connect debugger, set breakpoint after a `uart_print` call
3. Step over the `uart_print`
4. Verify the output appears in the UART console immediately, not after continue
