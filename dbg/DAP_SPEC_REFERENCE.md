DAP Specification: https://microsoft.github.io/debug-adapter-protocol/specification
JSON Schema: https://microsoft.github.io/debug-adapter-protocol/debugAdapterProtocol.json
Changelog: https://microsoft.github.io/debug-adapter-protocol/changelog

The full spec is too large to store as a local file (~3000 lines).
Reference the URLs above when implementing the DAP adapter.

## Minimum required requests for a working VS Code debug session:

- initialize → return capabilities
- attach → connect to target
- configurationDone → target ready
- threads → return at least one thread
- stackTrace → return frames (PC + source location)
- scopes → return scope references (Locals, Registers)
- variables → return variable names and values
- setBreakpoints → set breakpoints for a source file
- continue → resume execution
- next → step over
- stepIn → step into
- stepOut → step out
- pause → halt running target
- evaluate → expression evaluation (hover, watch, debug console)
- disconnect → clean up

## Required events:

- initialized → sent after initialize response
- stopped → sent when target stops (breakpoint, step, pause)
- terminated → sent when debug session ends

## Protocol framing:

Content-Length: <N>\r\n
\r\n
<N bytes of JSON>

Same framing as LSP (Language Server Protocol).
