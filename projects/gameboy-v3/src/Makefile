# projects/gameboy-v3/src/Makefile — builds the canvas userspace: the SDK (compiled
# into each display client), the compositor + its backend, appletd, services, games.
# Standalone build — NO forge integration: `make` / `make O=/tmp/o`. Cross-build by
# pointing CC at the musl toolchain: `make CC=arm-...-musleabihf-gcc`.
#
# CANVAS_BACKEND selects the compositor's display+input backend:
#   drm (default) — the T113 (DRM/KMS + evdev); what the flashable image builds.
#   web           — the host SIMULATOR (a browser over WebSocket). `make sim` uses this.
#
# NB: keeps its own -Iinclude via ALL_CFLAGS (a command-line CFLAGS= would override += ).

CC             ?= cc
# Out-of-source into the PRODUCT build tree (../build), namespaced under sim/ so these host binaries
# don't mix with forge's cross-compiled output (build/nodes,output,bundles,...). gitignored. forge's
# console recipe overrides O= to build/nodes/console/build for the ARM cross build.
O              ?= ../build/sim
DESTDIR        ?=
BINDIR         ?= /usr/bin
CANVAS_BACKEND ?= drm
SOUND_BACKEND  ?= file       # soundd output: file (host test) | web (browser) | alsa (T113, stubbed)

# The engine is now a first-class top-level module (../../engine), built to libengine.a and LINKED here
# (was compiled inline). runtime/ = the engine core (+ vendored third_party); consumers add its includes.
ENGINE_DIR  := $(abspath $(CURDIR)/../../../engine)
ENGINE_LIB  := $(O)/libengine.a
INC         := -Iinclude -I$(ENGINE_DIR)/runtime -I$(ENGINE_DIR)/third_party
WARN        := -Wall -Wextra
ALL_CFLAGS  := $(INC) $(WARN) -O2 $(CFLAGS)
ALL_LDFLAGS := $(LDFLAGS)

SDK    := libcanvas/canvas.c     # compiled into every display client (no separate .so yet)
# Games link: game.c + platform (libcanvas SDK, libsound) + libengine.a. libsound stays a platform lib
# (the audio client the engine calls), like libcanvas. cJSON now lives inside libengine.a.
ENGINE := libsound/sound.c platform_canvas.c $(ENGINE_LIB)

# Build the engine lib via its own Makefile, forwarding OUR toolchain (host sim OR forge cross) so
# engine + game are ABI-identical. PLATFORM_INC (absolute) points the still-coupled runtime at
# canvas.h/sound.h until the platform seam (Phase 2). Only rebuilds when engine sources change.
ENGINE_SRC := $(wildcard $(ENGINE_DIR)/runtime/*.c $(ENGINE_DIR)/runtime/*.h $(ENGINE_DIR)/third_party/*.c $(ENGINE_DIR)/third_party/*.h)
$(ENGINE_LIB): $(ENGINE_SRC) | $(O)
	@$(MAKE) --no-print-directory -C $(ENGINE_DIR) lib O='$(abspath $(O))' CC='$(CC)' CFLAGS='$(WARN) -O2 $(CFLAGS)'

# the compositor = canvasd + exactly ONE backend (+ the WebSocket server for web).
CANVASD_SRC  := compositor/canvasd.c compositor/backend_$(CANVAS_BACKEND).c
CANVASD_LIBS :=
ifeq ($(CANVAS_BACKEND),web)
CANVASD_SRC  += compositor/websocket.c
CANVASD_LIBS += -lz          # host sim only: zlib to deflate frames before the WebSocket
endif

# soundd = the audio server (soundd/) + exactly ONE audio backend (mirrors canvasd's backend split).
# The web backend reuses the compositor's websocket.c (shared util) — hence -Icompositor below.
SOUNDD_SRC  := soundd/soundd.c soundd/sound_backend_$(SOUND_BACKEND).c
SOUNDD_LIBS :=
ifeq ($(SOUND_BACKEND),web)
SOUNDD_SRC  += compositor/websocket.c
endif
# NB: the alsa backend is a silent STUB today (no ALSA calls) so it links nothing. The real T113
# impl (Phase 4) will `SOUNDD_LIBS += -lasound` — provided by the target sysroot on the cross-build.

# Games link libm — the engine's text.c uses stb_truetype (vector font rasterizer).
GAME_LIBS := -lm

BINS := $(O)/canvasd $(O)/soundd $(O)/appletd $(O)/canvas-launcher $(O)/powerd $(O)/canvas-breakout $(O)/canvas-shmup $(O)/canvas-platformer $(O)/canvas-adventure $(O)/canvas-asteroids $(O)/canvas-pong $(O)/canvas-pinball $(O)/canvas-billiards $(O)/canvas-td $(O)/canvas-raycast $(O)/canvas-flappy $(O)/canvas-racer $(O)/canvas-rhythm $(O)/canvas-doom $(O)/canvas-nova $(O)/canvas-circuit $(O)/canvas-kart $(O)/canvas-editor

all: $(BINS)

$(O):
	mkdir -p $(O)

# canvasd embeds exactly ONE backend, but it's the same target file for either — so make
# can't tell CANVAS_BACKEND changed (source mtimes are unchanged) and would keep a stale
# backend. A per-backend stamp forces a rebuild whenever you switch drm <-> web.
BACKEND_STAMP := $(O)/.backend-$(CANVAS_BACKEND)
$(BACKEND_STAMP): | $(O)
	@rm -f $(O)/.backend-*
	@touch $@

$(O)/canvasd: $(CANVASD_SRC) compositor/backend.h include/canvas-proto.h $(BACKEND_STAMP) | $(O)
	$(CC) $(ALL_CFLAGS) $(CANVASD_SRC) $(ALL_LDFLAGS) $(CANVASD_LIBS) -o $@

# soundd embeds one audio backend; a per-backend stamp forces a rebuild on file<->web<->alsa switch.
SOUND_STAMP := $(O)/.sound-backend-$(SOUND_BACKEND)
$(SOUND_STAMP): | $(O)
	@rm -f $(O)/.sound-backend-*
	@touch $@

$(O)/soundd: $(SOUNDD_SRC) soundd/sound_backend.h include/sound-proto.h $(SOUND_STAMP) | $(O)
	$(CC) $(ALL_CFLAGS) -Icompositor $(SOUNDD_SRC) $(ALL_LDFLAGS) $(SOUNDD_LIBS) -o $@

# appletd (application/applet manager) uses only the appletd control protocol — no SDK.
$(O)/appletd: appletd/appletd.c include/appletd-proto.h | $(O)
	$(CC) $(ALL_CFLAGS) $< $(ALL_LDFLAGS) -o $@

$(O)/canvas-launcher: launcher/launcher.c $(SDK) include/canvas.h include/canvas-proto.h include/appletd-proto.h | $(O)
	$(CC) $(ALL_CFLAGS) launcher/launcher.c $(SDK) $(ALL_LDFLAGS) -o $@

$(O)/powerd: services/powerd.c | $(O)
	$(CC) $(ALL_CFLAGS) $< $(ALL_LDFLAGS) -o $@

ENGINE_DEPS := $(ENGINE) $(SDK) $(ENGINE_DIR)/runtime/engine.h $(ENGINE_DIR)/third_party/cJSON.h include/canvas.h include/canvas-proto.h include/sound.h include/sound-proto.h

$(O)/canvas-breakout: games/breakout/breakout.c $(ENGINE_DEPS) | $(O)
	$(CC) $(ALL_CFLAGS) games/breakout/breakout.c $(ENGINE) $(SDK) $(ALL_LDFLAGS) $(GAME_LIBS) -o $@

$(O)/canvas-shmup: games/shmup/shmup.c $(ENGINE_DEPS) | $(O)
	$(CC) $(ALL_CFLAGS) games/shmup/shmup.c $(ENGINE) $(SDK) $(ALL_LDFLAGS) $(GAME_LIBS) -o $@

$(O)/canvas-platformer: games/platformer/platformer.c $(ENGINE_DEPS) | $(O)
	$(CC) $(ALL_CFLAGS) games/platformer/platformer.c $(ENGINE) $(SDK) $(ALL_LDFLAGS) $(GAME_LIBS) -o $@

$(O)/canvas-adventure: games/adventure/adventure.c $(ENGINE_DEPS) | $(O)
	$(CC) $(ALL_CFLAGS) games/adventure/adventure.c $(ENGINE) $(SDK) $(ALL_LDFLAGS) $(GAME_LIBS) -o $@

$(O)/canvas-asteroids: games/asteroids/asteroids.c $(ENGINE_DEPS) | $(O)
	$(CC) $(ALL_CFLAGS) games/asteroids/asteroids.c $(ENGINE) $(SDK) $(ALL_LDFLAGS) $(GAME_LIBS) -o $@

$(O)/canvas-pong: games/pong/pong.c $(ENGINE_DEPS) | $(O)
	$(CC) $(ALL_CFLAGS) games/pong/pong.c $(ENGINE) $(SDK) $(ALL_LDFLAGS) $(GAME_LIBS) -o $@

$(O)/canvas-pinball: games/pinball/pinball.c $(ENGINE_DEPS) | $(O)
	$(CC) $(ALL_CFLAGS) games/pinball/pinball.c $(ENGINE) $(SDK) $(ALL_LDFLAGS) $(GAME_LIBS) -o $@

$(O)/canvas-billiards: games/billiards/billiards.c $(ENGINE_DEPS) | $(O)
	$(CC) $(ALL_CFLAGS) games/billiards/billiards.c $(ENGINE) $(SDK) $(ALL_LDFLAGS) $(GAME_LIBS) -o $@

$(O)/canvas-td: games/td/td.c $(ENGINE_DEPS) | $(O)
	$(CC) $(ALL_CFLAGS) games/td/td.c $(ENGINE) $(SDK) $(ALL_LDFLAGS) $(GAME_LIBS) -o $@

$(O)/canvas-raycast: games/raycast/raycast.c $(ENGINE_DEPS) | $(O)
	$(CC) $(ALL_CFLAGS) games/raycast/raycast.c $(ENGINE) $(SDK) $(ALL_LDFLAGS) $(GAME_LIBS) -o $@

$(O)/canvas-flappy: games/flappy/flappy.c $(ENGINE_DEPS) | $(O)
	$(CC) $(ALL_CFLAGS) games/flappy/flappy.c $(ENGINE) $(SDK) $(ALL_LDFLAGS) $(GAME_LIBS) -o $@

$(O)/canvas-racer: games/racer/racer.c $(ENGINE_DEPS) | $(O)
	$(CC) $(ALL_CFLAGS) games/racer/racer.c $(ENGINE) $(SDK) $(ALL_LDFLAGS) $(GAME_LIBS) -o $@

$(O)/canvas-rhythm: games/rhythm/rhythm.c $(ENGINE_DEPS) | $(O)
	$(CC) $(ALL_CFLAGS) games/rhythm/rhythm.c $(ENGINE) $(SDK) $(ALL_LDFLAGS) $(GAME_LIBS) -o $@

$(O)/canvas-doom: games/doom/doom.c $(ENGINE_DEPS) | $(O)
	$(CC) $(ALL_CFLAGS) games/doom/doom.c $(ENGINE) $(SDK) $(ALL_LDFLAGS) $(GAME_LIBS) -o $@

$(O)/canvas-nova: games/nova/nova.c $(ENGINE_DEPS) | $(O)
	$(CC) $(ALL_CFLAGS) games/nova/nova.c $(ENGINE) $(SDK) $(ALL_LDFLAGS) $(GAME_LIBS) -o $@

$(O)/canvas-circuit: games/circuit/circuit.c $(ENGINE_DEPS) | $(O)
	$(CC) $(ALL_CFLAGS) games/circuit/circuit.c $(ENGINE) $(SDK) $(ALL_LDFLAGS) $(GAME_LIBS) -o $@

$(O)/canvas-kart: games/kart/kart.c $(ENGINE_DEPS) | $(O)
	$(CC) $(ALL_CFLAGS) games/kart/kart.c $(ENGINE) $(SDK) $(ALL_LDFLAGS) $(GAME_LIBS) -o $@

# the track editor — a canvas client (viewable in the browser via the web backend)
$(O)/canvas-editor: editor.c $(ENGINE_DEPS) | $(O)
	$(CC) $(ALL_CFLAGS) editor.c $(ENGINE) $(SDK) $(ALL_LDFLAGS) $(GAME_LIBS) -o $@

# sim: host simulator — build with the web backend, run a game, view in a browser.
#   make sim CLIENT=breakout        # game is required; the canvas- prefix is optional
.PHONY: sim
sim:
	$(MAKE) CANVAS_BACKEND=web SOUND_BACKEND=web
	@echo; ../run-sim.sh $(CLIENT)

install: all
	install -d $(DESTDIR)$(BINDIR)
	install -m 0755 $(O)/canvasd        $(DESTDIR)$(BINDIR)/canvasd
	install -m 0755 $(O)/soundd         $(DESTDIR)$(BINDIR)/soundd
	install -m 0755 $(O)/appletd         $(DESTDIR)$(BINDIR)/appletd
	install -m 0755 $(O)/canvas-launcher $(DESTDIR)$(BINDIR)/canvas-launcher
	install -m 0755 $(O)/powerd          $(DESTDIR)$(BINDIR)/powerd
	install -m 0755 $(O)/canvas-breakout $(DESTDIR)$(BINDIR)/canvas-breakout
	install -m 0755 $(O)/canvas-shmup    $(DESTDIR)$(BINDIR)/canvas-shmup
	install -m 0755 $(O)/canvas-platformer $(DESTDIR)$(BINDIR)/canvas-platformer
	install -m 0755 $(O)/canvas-adventure $(DESTDIR)$(BINDIR)/canvas-adventure
	install -m 0755 $(O)/canvas-asteroids $(DESTDIR)$(BINDIR)/canvas-asteroids
	install -m 0755 $(O)/canvas-pong      $(DESTDIR)$(BINDIR)/canvas-pong
	install -m 0755 $(O)/canvas-pinball   $(DESTDIR)$(BINDIR)/canvas-pinball
	install -m 0755 $(O)/canvas-billiards $(DESTDIR)$(BINDIR)/canvas-billiards
	install -m 0755 $(O)/canvas-td        $(DESTDIR)$(BINDIR)/canvas-td
	install -m 0755 $(O)/canvas-raycast   $(DESTDIR)$(BINDIR)/canvas-raycast
	install -m 0755 $(O)/canvas-flappy    $(DESTDIR)$(BINDIR)/canvas-flappy
	install -m 0755 $(O)/canvas-racer     $(DESTDIR)$(BINDIR)/canvas-racer
	install -m 0755 $(O)/canvas-rhythm    $(DESTDIR)$(BINDIR)/canvas-rhythm
	install -m 0755 $(O)/canvas-doom      $(DESTDIR)$(BINDIR)/canvas-doom
	install -m 0755 $(O)/canvas-nova      $(DESTDIR)$(BINDIR)/canvas-nova
	install -m 0755 $(O)/canvas-circuit   $(DESTDIR)$(BINDIR)/canvas-circuit
	install -m 0755 $(O)/canvas-kart      $(DESTDIR)$(BINDIR)/canvas-kart
	install -d $(DESTDIR)/usr/share/canvas
	cp -R assets/. $(DESTDIR)/usr/share/canvas/     # per-game asset folders: common/, shmup/, platformer/, …

clean:
	rm -rf $(O)

.PHONY: all install clean