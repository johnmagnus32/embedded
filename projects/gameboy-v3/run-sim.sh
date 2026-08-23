#!/bin/sh
# run-sim.sh — host simulator for the gameboy-v3 console: canvasd (web backend) + one game,
# viewable in a browser. Build first:  make -C src CANVAS_BACKEND=web
#
# Usage (from the project dir):  ./run-sim.sh <game>
#   <game> is the short name, e.g.  ./run-sim.sh breakout   (runs canvas-breakout).
#   The "canvas-" prefix is optional — "breakout" and "canvas-breakout" both work.
#
# Serves the UI on http://localhost:$CANVAS_WEB_PORT (default 8080), loopback only; uses a
# /tmp socket so it runs as a normal user.
set -e
PDIR=$(cd "$(dirname "$0")" && pwd)     # projects/gameboy-v3 (this script lives at the project root)
O=${O:-$PDIR/build/sim}                 # host-sim binaries (matches the src/Makefile default)

usage() {
	echo "usage: $(basename "$0") <game>    e.g. $(basename "$0") breakout" >&2
	if [ -d "$O" ]; then
		games=$(cd "$O" && ls canvas-* 2>/dev/null | sed 's/^canvas-//' | tr '\n' ' ')
		[ -n "$games" ] && echo "available games: $games" >&2
	fi
	exit 2
}

[ $# -ge 1 ] || usage
name=${1#canvas-}                       # accept "breakout" or "canvas-breakout"
CLIENT=canvas-$name

PORT=${CANVAS_WEB_PORT:-8080}
export CANVAS_WEB_PORT=$PORT
export CANVAS_SOCK=${CANVAS_SOCK:-/tmp/canvas-sim.sock}

# Font for the vector-text engine (stb_truetype). Point CANVAS_FONT at any .ttf to change the
# look; default to the bundled asset under src/.
export CANVAS_FONT=${CANVAS_FONT:-$PDIR/src/assets/common/ui.ttf}
# Asset base — games load per-game paths relative to this (e.g. "shmup/ship.png", "platformer/level1.tmj").
export CANVAS_ASSETS=${CANVAS_ASSETS:-$PDIR/src/assets}

[ -x "$O/canvasd" ]  || { echo "build first: make -C src CANVAS_BACKEND=web" >&2; exit 1; }
[ -x "$O/$CLIENT" ]  || { echo "no such game '$name' ($O/$CLIENT not found)" >&2; usage; }

rm -f "$CANVAS_SOCK"
"$O/canvasd" &
CANVASD=$!
trap 'kill $CANVASD 2>/dev/null' EXIT INT TERM

# wait for the compositor socket
i=0; while [ ! -S "$CANVAS_SOCK" ] && [ "$i" -lt 50 ]; do i=$((i+1)); sleep 0.1; done

cat <<EOF

──────────────── canvas simulator ────────────────
 game   : $name
 view   : http://localhost:$PORT
 remote : ssh -L $PORT:localhost:$PORT <this-cloud-box>   then open the URL on your Mac
 keys   : arrows = D-pad,  z or Space = A (jump/action),  x=B a=X s=Y,  q=L w=R,  Enter=Start, Shift=Select
 stop   : Ctrl-C
───────────────────────────────────────────────────

EOF

# Run the client in the FOREGROUND — do NOT exec. exec would replace this shell and
# drop the trap above, so Ctrl-C would kill only the client and orphan canvasd (leaking
# the port). Running it as a child keeps the EXIT/INT/TERM trap live to reap canvasd.
"$O/$CLIENT"
