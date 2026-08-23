# gameboy-v3 console userspace — `canvas` (`src/`)

Model-B console stack: the compositor (`canvasd`) is the permanent DRM master and
owns the screen; everything else is a **client** that renders into a
compositor-allocated buffer and hands it back. One screen, at most two DE planes
(game + overlay), no GPU. The runtime is named **canvas**; it lives under
`projects/gameboy-v3/` for now but isn't tied to this board — only the input map,
sysfs paths, audio routing, and panel geometry are board-specific. Hardware surface:
`../pcb/BOM.md`.

## Layout
| path | what |
|---|---|
| `include/canvas-proto.h` | compositor↔client wire protocol (the spine) |
| `include/canvas.h`       | client SDK public API |
| `libcanvas/`             | SDK impl — connect, buffers, submit, input |
| `include/engine.h` + `engine/` | 2D engine: loop + shapes + input + TrueType text + a Godot-style scene graph (nodes/sprites) |
| `include/appletd-proto.h`| appletd control protocol (launch/lifecycle) |
| `compositor/`            | `canvasd` — DRM master, DE planes, focus (no spawning) |
| `appletd/`               | application/applet manager — spawns/reaps games (Switch am+pm) |
| `launcher/`              | the home menu (a client; launches games via appletd) |
| `services/`              | `powerd` (the pattern for audiod/hapticd/btd/libraryd) |
| `games/breakout/`, `games/shmup/` | sample games on the engine (shmup uses the scene graph) |

## Substrate
Needs a complete libc + mainline drivers (DRM/evdev/ALSA/`dlopen`/pthread), which the
from-scratch custom kernel/libc don't provide — so it targets the musl/mainline flavor.

## Build
    make                        # host build -> ../build/sim/  (canvasd, appletd, ..., canvas-breakout)
    make sim CLIENT=breakout    # build the web backend + launch the host simulator (../run-sim.sh)
    make CC=arm-buildroot-linux-musleabihf-gcc     # cross-build for the board
    make O=/tmp/o DESTDIR=/tmp/root install

Two paths share this Makefile: `make` here is the STANDALONE host build (out-of-source into
`../build/sim`, for the simulator / dev). Forge's console package (`../packages/console/recipe.sh`)
cross-builds the SAME source into the product image, overriding `O=` (-> `build/nodes/console/build`)
and `DESTDIR=`.

## Mock status
**Real:** the wire protocol, SDK buffer/input bookkeeping, the compositor
socket/focus loop, `appletd`'s launch/reap/one-app-at-a-time, the engine (text + scene graph), the sample games.
**Stubbed (`TODO` in-file):** KMS bring-up + DE-plane page-flip, evdev read + system-
chord filtering, ALSA audio, and powerd's idle/power machine.
