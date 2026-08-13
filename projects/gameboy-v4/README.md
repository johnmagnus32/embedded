# gameboy-v4 — Allwinner H616 3D-capable handheld (RESEARCH / PLANNING)

**Status:** 🔬 Research + planning only. **Nothing built, nothing on silicon.** No
code, no board ordered. This directory currently holds the hardware planning for a
v4 that adds **real 3D graphics** ("Mario Kart"-class) on top of the v3 Linux handheld.

## Where this sits in the series

| | Compute | Graphics | OS |
|---|---|---|---|
| v1 | STM32F411 (Cortex-M4) | software 2D | own RTOS, bare-metal |
| v2 | STM32F411 + iCE40 FPGA | hardware sprite PPU (CPU + coprocessor split) | own RTOS |
| v3 | Allwinner T113-S3 (dual A7, ARMv7, 128 MB **SiP** DDR3) | CPU/framebuffer, **no GPU** | mainline Linux **+** full from-scratch stack |
| **v4** | **Allwinner H616** (quad A53, **AArch64**, external DDR3) | **Mali-G31 MP2 + open Panfrost (GLES 3.1)** | mainline Linux (GPU) + from-scratch stack (AArch64 port) |

## Why H616 (the short version)

The full decision history — including why NOT RK3566 (HDI-forced), Tegra X1
(closed/off-thesis), R40/STM32MP157 (ARMv7 but weaker GPU) — lives in the
assistant's project memory. The load-bearing facts for v4:

- **3D blocker is a rasterizer, not RAM.** The T113 has no GPU; the H616's
  **Mali-G31 MP2** on the open, mainline **Panfrost** driver gives GLES 3.1 —
  enough for N64-class (Tier-2) textured/Z-buffered 3D and light modern shader work.
- **Fully JLC-buildable as a BARE CHIP.** H616 = TFBGA284, 14×12 mm, uniform
  **0.65 mm pitch → no HDI**. JLC-standard fab (through-hole vias) + JLC SMT
  assembly place it. Proven by an open 4-layer reference design (below).
- **The costs, eyes open:** (1) **AArch64** — the from-scratch bootloader/kernel/
  libc/linker/coreutils need a real 32→64-bit re-port (the GIC-400/GICv2 seam
  carries over; the CPU-arch layer does not). (2) **External DDR3 routing** — a new
  signal-integrity skill the T113's in-package SiP DRAM never required.
- 3D always rides the **mainline-Linux + Mesa/Panfrost** path — the from-scratch
  kernel never touches the GPU. So B1 (mainline bring-up) is enough to start writing
  graphics; the AArch64 port (B2) is a parallel OS-understanding track.

## Media capability — browser + video (verified 2026, IMPORTANT)

If "browse the web / watch YouTube" is a goal, know the honest limits on the
**mainline + open-driver** stack this project requires (all verified against the
torvalds/linux source + sunxi/kernel docs):

- **Browsing:** works but **Raspberry-Pi-3B+ class** — a browser renders, but heavy
  JS sites feel sluggish (A53 IPC ceiling; the GPU accelerates compositing, not JS).
- **Video decode — the real gap, and it's a DRIVER gap not a silicon one:**
  - The open mainline **Cedrus** VPU driver decodes only **MPEG-2, H.264, H.265/HEVC,
    VP8**. **VP9 has NEVER been merged for ANY Allwinner SoC** (no `cedrus_vp9.c`, no
    capability bit) — and **VP9 is YouTube's default codec**. **AV1** has no Cedrus
    support and **no H616 silicon block** at all (software-only → not real-time).
  - **The H616 is not even bound into mainline Cedrus today** — no entry in the
    driver's `of_device_id` table, no video-engine node in `sun50i-h616.dtsi`. So on
    a pure-mainline stack the H616 VPU is currently **dark (zero HW decode of any
    codec)** until someone adds an H616 variant (which would inherit H6-class
    MPEG2/H264/HEVC/VP8 — **still no VP9**).
- **What this means for YouTube:** in a **browser** it silently **software-decodes**
  (VP9) → **~360–480p** on the quad A53, hot. The route that actually HW-decodes is
  **`mpv` + `yt-dlp` forcing H.264** through v4l2-request/drm_prime → **~1080p** — but
  that's a terminal player, not a browser, and needs the H616 Cedrus port first.
- **RAM — the one workload where it IS the blocker:** a modern browser wants **4 GB**.
  1 GB thrashes after 1–3 tabs. **This is the argument for building the reference's
  4 GB LPDDR4 variant instead of the 1 GB DDR3 one** — the only use case where more
  RAM matters (3D gaming and video decode are NOT RAM-limited). Tradeoff: LPDDR4
  sourcing is thinner at LCSC than DDR3 (see [`pcb/BOM.md`](pcb/BOM.md)).

> **Net:** the H616 is a fine *light Linux + retro-gaming + 3D* handheld, but a weak
> *open-stack YouTube* device. Smooth browser-YouTube is effectively unavailable on
> mainline; plan on `mpv`+`yt-dlp` (H.264, 1080p) or accept 480p software in-browser.

## Reference design (the starting point)

The board core is cloned, not designed from scratch. Reference:

- **Kononenko-K `Allwinner_H616_Devboard`** — <https://github.com/Kononenko-K/Allwinner_H616_Devboard>
- **Native KiCad 8** (`version 20240108`), **CERN-OHL-P** (permissive), **4-layer**.
- Two variants: **`H616_DDR3`** (1 GB DDR3, AXP305 PMIC — *the one we use*) and
  `H616_LPDDR4` (4 GB LPDDR4, AXP313A). Derived from Orange Pi Zero 2 / Zero 3.
- The DDR3 topology + power tree are the hard, must-be-exact parts — **copy them
  net-for-net.** Our handheld peripherals (LCD, buttons, analog stick, audio, battery)
  are the easy zone we add on top.

## Build plan (de-risking order)

1. **Reproduce the reference board UNCHANGED** → prove fab / assembly / DDR bring-up.
2. **Mainline software on it** (U-Boot + Linux + BusyBox) → prove boot + shell.
3. **Desktop GLES2 renderer in parallel** (chip-agnostic) → learn graphics, zero board risk.
4. **Our handheld board** → graft LCD/input/audio/power onto the proven SoC core.
5. **Panfrost + our renderer on silicon** → first 3D.
6. **From-scratch AArch64 port** → the OS-understanding headline, off the critical path.

## Contents

- [`pcb/BOM.md`](pcb/BOM.md) — the **base SoC + DDR + power + clock core** BOM,
  extracted from the reference design. This is the "sacred core" to copy verbatim;
  handheld feature parts (screen/haptics/audio/battery) are TODO, to be added like
  the [gameboy-v3 BOM](../gameboy-v3/pcb/BOM.md) did on top of its proven core.
