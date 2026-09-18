# engine/platform/ — the platform seam (Phase 2, not yet built)

The engine core (`runtime/`) currently `#include`s `canvas.h` / `sound.h` directly — it's coupled to
the gameboy-v3 canvas compositor for its framebuffer, input, and audio out. That's the one backwards
dependency left after the relocate.

Phase 2 replaces that with an interface here — `engine_platform.h` (acquire-frame / submit /
poll-input / write-audio) — that `runtime/` calls instead of `canvas_*`. Then each front-end provides
an implementation:

- `platform/canvas/` — the on-device/sim backend (registers the canvas compositor client),
- a host backend for the editor.

After Phase 2 the runtime depends on nothing project-specific, and `PLATFORM_INC` disappears.
