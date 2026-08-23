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

INC         := -Iinclude -Ithird_party
WARN        := -Wall -Wextra
ALL_CFLAGS  := $(INC) $(WARN) -O2 $(CFLAGS)
ALL_LDFLAGS := $(LDFLAGS)

SDK    := libcanvas/canvas.c     # compiled into every display client (no separate .so yet)
ENGINE := engine/engine.c engine/text.c engine/scene.c engine/level.c engine/physics.c third_party/cJSON.c   # + cJSON

# the compositor = canvasd + exactly ONE backend (+ the WebSocket server for web).
CANVASD_SRC  := compositor/canvasd.c compositor/backend_$(CANVAS_BACKEND).c
CANVASD_LIBS :=
ifeq ($(CANVAS_BACKEND),web)
CANVASD_SRC  += compositor/websocket.c
CANVASD_LIBS += -lz          # host sim only: zlib to deflate frames before the WebSocket
endif

# Games link libm — the engine's text.c uses stb_truetype (vector font rasterizer).
GAME_LIBS := -lm

BINS := $(O)/canvasd $(O)/appletd $(O)/canvas-launcher $(O)/powerd $(O)/canvas-breakout $(O)/canvas-shmup $(O)/canvas-platformer $(O)/canvas-adventure

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

# appletd (application/applet manager) uses only the appletd control protocol — no SDK.
$(O)/appletd: appletd/appletd.c include/appletd-proto.h | $(O)
	$(CC) $(ALL_CFLAGS) $< $(ALL_LDFLAGS) -o $@

$(O)/canvas-launcher: launcher/launcher.c $(SDK) include/canvas.h include/appletd-proto.h | $(O)
	$(CC) $(ALL_CFLAGS) launcher/launcher.c $(SDK) $(ALL_LDFLAGS) -o $@

$(O)/powerd: services/powerd.c | $(O)
	$(CC) $(ALL_CFLAGS) $< $(ALL_LDFLAGS) -o $@

ENGINE_DEPS := $(ENGINE) $(SDK) include/engine.h include/canvas.h include/canvas-proto.h engine/engine_internal.h third_party/stb_truetype.h third_party/stb_image.h third_party/cJSON.h

$(O)/canvas-breakout: games/breakout/breakout.c $(ENGINE_DEPS) | $(O)
	$(CC) $(ALL_CFLAGS) games/breakout/breakout.c $(ENGINE) $(SDK) $(ALL_LDFLAGS) $(GAME_LIBS) -o $@

$(O)/canvas-shmup: games/shmup/shmup.c $(ENGINE_DEPS) | $(O)
	$(CC) $(ALL_CFLAGS) games/shmup/shmup.c $(ENGINE) $(SDK) $(ALL_LDFLAGS) $(GAME_LIBS) -o $@

$(O)/canvas-platformer: games/platformer/platformer.c $(ENGINE_DEPS) | $(O)
	$(CC) $(ALL_CFLAGS) games/platformer/platformer.c $(ENGINE) $(SDK) $(ALL_LDFLAGS) $(GAME_LIBS) -o $@

$(O)/canvas-adventure: games/adventure/adventure.c $(ENGINE_DEPS) | $(O)
	$(CC) $(ALL_CFLAGS) games/adventure/adventure.c $(ENGINE) $(SDK) $(ALL_LDFLAGS) $(GAME_LIBS) -o $@

# sim: host simulator — build with the web backend, run a game, view in a browser.
#   make sim CLIENT=breakout        # game is required; the canvas- prefix is optional
.PHONY: sim
sim:
	$(MAKE) CANVAS_BACKEND=web
	@echo; ../run-sim.sh $(CLIENT)

install: all
	install -d $(DESTDIR)$(BINDIR)
	install -m 0755 $(O)/canvasd        $(DESTDIR)$(BINDIR)/canvasd
	install -m 0755 $(O)/appletd         $(DESTDIR)$(BINDIR)/appletd
	install -m 0755 $(O)/canvas-launcher $(DESTDIR)$(BINDIR)/canvas-launcher
	install -m 0755 $(O)/powerd          $(DESTDIR)$(BINDIR)/powerd
	install -m 0755 $(O)/canvas-breakout $(DESTDIR)$(BINDIR)/canvas-breakout
	install -m 0755 $(O)/canvas-shmup    $(DESTDIR)$(BINDIR)/canvas-shmup
	install -m 0755 $(O)/canvas-platformer $(DESTDIR)$(BINDIR)/canvas-platformer
	install -m 0755 $(O)/canvas-adventure $(DESTDIR)$(BINDIR)/canvas-adventure
	install -d $(DESTDIR)/usr/share/canvas
	cp -R assets/. $(DESTDIR)/usr/share/canvas/     # per-game asset folders: common/, shmup/, platformer/, …

clean:
	rm -rf $(O)

.PHONY: all install clean