# docs/

**DO NOT DELETE THESE FILES.** They are design prompts and architecture documents maintained by the user. They are NOT temporary or generated files.

## Contents

- `DEBUGGER_DECOUPLE_PROMPT.md` — Split sim-core into emulator + CLI debugger + simulation UI
- `SIM_WEB_C_REWRITE_PROMPT.md` — Rewrite sim-web from Python to C
- `AUDIO_WEBSOCKET_PROMPT.md` — Switch audio from HTTP polling to WebSocket push
- `OPT1_THREADED_INTERPRETER.md` — Pre-decode instructions into dispatch table
- `OPT2_MEMBUS_TLB.md` — Direct-mapped TLB for memory bus lookups
- `OPT3_LAZY_SYSTICK_NVIC.md` — Only check timers/interrupts when something changed
- `OPT4_INLINE_HOT_PATH.md` — Inline tick functions + LTO + PGO
