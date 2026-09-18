# engine/editor — World Builder: plan & decisions

## What this is
A **host-only** visual editor for building game worlds with the *same engine renderer the game uses*
— WYSIWYG: what you build is what you play. It reads/writes the same asset files the game loads
(today: Tiled `.tmj` tile maps; later: heightmaps / meshes / spline data). It is a **native desktop
app** you run locally (e.g. on a Mac), kept in sync with the cloud dev box over **git**.

## Core invariants (do not break)
- **One-way dependency:** editor → engine core (`runtime/`, `libengine.a`). The core NEVER depends on the editor.
- **Never on the device:** the editor is a HOST target only. The flashed image contains `runtime/` + the game, never `editor/`.
- **The viewport is engine-rendered.** The world (tiles now, 3D terrain later) is drawn by the engine's
  software renderer into a pixel buffer — never reimplemented in the UI toolkit. This is what makes it
  WYSIWYG and what lets it scale to 3D.
- **Chrome is a thin, swappable shell.** Menus/panels/buttons *call into* the editor's document/tool
  logic; they don't own it.

## Architecture (three layers)
1. **Engine core** (`runtime/` → `libengine.a`) — rasterizers, tilemap/level load, math. Shared with the
   game. Platform-agnostic.
2. **Editor core** (`editor/`, portable **C**) — the document (map model, camera, tools) + operations:
   load/save `.tmj`, paint cells, place objects, screen→world pick, (later) heightmap edit ops. Knows
   nothing about windows or widgets.
3. **Host platform + chrome** — a window/input/present backend behind the engine platform seam
   (`runtime/engine_platform.h`), plus the UI toolkit.

## Decisions

### Native (SDL2), not remote web
- We first built a remote-web path (server renders, streams pixels to a browser over HTTP+WebSocket).
  It worked but was **blurry** (fixed 800×480 upscaled on a Retina display) and **laggy** (streamed
  frames + ssh round-trip + a ~25 fps cap).
- Native renders **locally at the display's real resolution** → crisp + instant. That eliminates both
  problems for free.
- Portability is handled by **git** (push on the box, pull/build on the Mac), not by streaming.
- The remote-web platform (`platform/web/`) has been **removed**.

### SDL2 for window/input/present
- SDL = cross-platform window + input + "put this pixel buffer on screen." Thin: the engine keeps its
  own main loop; SDL is just a present/input adapter. See `platform/sdl/platform_sdl.c`.

### Dear ImGui for chrome (planned) — not wxWidgets, not hand-rolled
- A real editor needs menus, panels, buttons, text fields, lists, docking. Hand-rolling these on bare
  SDL is a dead end (today's toolbar is hand-rasterized rectangles — fine for a few buttons, not a real UI).
- **Dear ImGui**: composes with SDL, is a light drop-in (no build system, no native-toolkit dependency),
  gives all those widgets + docking, keeps the engine's loop, and looks like a game tool (Godot/Unity
  style) — which is right for a world editor. One cost: the chrome layer becomes **C++** (engine +
  editor core stay C).
- **wxWidgets / Qt — rejected:** heavy C++ native-toolkit dependency, takes over the app loop,
  re-couples the editor, and doesn't render the viewport anyway. Only worth it for a native-first,
  dialog-heavy product — not this.
- Widgets drive the editor-core functions (`setTool`/`selectTile`/`paint`/`save`); swapping chrome
  (ImGui ↔ anything else) never touches the engine or the editor core.

## Current state (Phase 0 — done)
- **`editor/editor.c`** — 2D Tiled `.tmj` world builder: loads a map into an editable id-grid; paints a
  `solid` (collision) layer + a `bg` decoration layer; places named entity objects; saves back to
  `.tmj` (preserving the tileset block so the game still renders textured). Pan (drag) + zoom
  (wheel/pinch/Up-Down). Tiles shown as flat palette colors.
  - Verified: loads the real `platformer/level1.tmj` (48×15, 7 objects). Save is code-verified (mirrors
    the loader) but **not yet round-trip-tested live**.
  - Chrome today: a self-drawn toolbar via engine primitives (to be replaced by ImGui in Phase 2).
- **`platform/sdl/platform_sdl.c`** — native SDL2 backend: HiDPI (renders at drawable pixels), resizable,
  mouse/keys/scroll/pinch → `eng_input_event`, window-close → `eng_quit()`. Reviewed; two bugs fixed
  (resize-realloc OOB; Makefile stale-binary on platform switch). **Not yet compiled on macOS.**
- **`runtime/engine.c` + `engine_platform.h`** — added `eng_wheel()` to the core + `ENG_EV_WHEEL` to the
  seam for scroll/pinch zoom.
- **`Makefile`** — `make editor PLATFORM=sdl` (default `sdl`); link-time platform selection with a
  `.platform` stamp that forces a relink when the platform changes.
- Font: text needs `CANVAS_FONT=<path to ui.ttf>` (e.g. `projects/gameboy-v3/src/assets/common/ui.ttf`).

## Roadmap

### Phase 1 — prove native on the Mac
- `brew install sdl2`; `cd engine && make lib && make editor PLATFORM=sdl`.
- Run against a real level; confirm crisp + instant + resize.
- Close the save loop: edit → save → run the platformer on that level (it's git-tracked, so safe to
  overwrite).

### Phase 2 — real chrome (Dear ImGui)
- Split `editor.c` into **editor core (C)** + **chrome (ImGui, C++)**; wire ImGui into the SDL backend.
- Menu bar (File: open / save / save-as; Edit) + docked panels: tool palette, tile palette, layer
  selector, object types, properties.
- Render the engine frame into a dockable **Viewport** panel (Unity/Godot layout).
- Native file open/save (ImGui file dialog or `tinyfiledialogs`).

### Phase 3 — editor features
- Map resize; delete/move objects; brush size/shapes; WYSIWYG **textured** tiles (load the tileset PNG,
  draw the real atlas cells); undo/redo; multiple open maps.

### Phase 4 — toward the world builder (2D → 3D)
- Generalize the document to authored terrain: a heightmap **3D viewport** (engine-rendered), a **sculpt
  brush** (raise/lower/smooth), and a **road/spline tool** — the terrain north-star — all behind the same
  editor-core / chrome split.

## Constraints
- Original assets only (no copyrighted IP).
- Keep builds warning-clean.
- One renderer for the viewport (the engine); the toolkit never draws world content.
