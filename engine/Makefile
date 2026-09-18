# engine/ — the standalone canvas engine (a first-class module, sibling of kernel/, libc/, rtos/…).
#
#   runtime/     the engine core -> libengine.a (ships on the device; linked by games AND the editor)
#   third_party/ vendored deps (stb_truetype, stb_image, cJSON)
#   platform/    the platform seam + backends (Phase 2 — see platform/README.md)
#   editor/      host-only world editor (later — see editor/README.md)
#
# Build:  make lib            -> $(O)/libengine.a
# Consumers pass CC / CFLAGS / O so the SAME toolchain builds engine + game (host sim OR cross device).
#
# PHASE 1 coupling (temporary): runtime/ still #includes canvas.h + sound.h (its platform). Until the
# platform seam (Phase 2) lands, the caller must point PLATFORM_INC at those headers, e.g.
#   make -C engine lib PLATFORM_INC=-I/abs/path/to/projects/gameboy-v3/src/include
# gameboy-v3's src/Makefile does this automatically. Phase 2 removes the need for PLATFORM_INC.

CC           ?= cc
O            ?= build
CFLAGS       ?= -O2 -Wall -Wextra
PLATFORM_INC ?=

EINC := -Iruntime -Ithird_party $(PLATFORM_INC)
ESRC := $(wildcard runtime/*.c) third_party/cJSON.c
EOBJ := $(patsubst %.c,$(O)/eng/%.o,$(ESRC))

.PHONY: lib editor clean FORCE
lib: $(O)/libengine.a

$(O)/libengine.a: $(EOBJ)
	@mkdir -p $(@D)
	ar rcs $@ $^

$(O)/eng/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(EINC) -c $< -o $@

# The standalone world editor (host-only): the editor app + ONE host platform, linked against
# libengine.a. editor/editor.c is platform-agnostic (only the public eng_* API), so the platform is a
# LINK-TIME choice — a new host is just another PLAT_* block, the engine + editor never change.
# Today the only host is native SDL2 (crisp + instant, runs locally e.g. on a Mac):
#   make editor                 (PLATFORM=sdl by default)
# Run: ./build/editor <path-to.tmj>   (text needs CANVAS_FONT=<path to ui.ttf>)
PLATFORM ?= sdl
ifeq ($(PLATFORM),sdl)
  PLAT_SRC := platform/sdl/platform_sdl.c
  PLAT_INC := $(shell sdl2-config --cflags 2>/dev/null || pkg-config --cflags sdl2 2>/dev/null)
  PLAT_LIB := $(shell sdl2-config --libs 2>/dev/null || pkg-config --libs sdl2 2>/dev/null)
else
  $(error unknown PLATFORM '$(PLATFORM)' — only 'sdl' is available)
endif
EDITOR_SRC := editor/editor.c $(PLAT_SRC)
editor: $(O)/editor
# $(O)/editor is a shared output path, so switching PLATFORM must force a relink — make compares
# mtimes, not that the prereq LIST changed. The .platform stamp rewrites (bumping its mtime) ONLY
# when PLATFORM differs from last build, forcing a relink on switch while staying a no-op otherwise.
$(O)/.platform: FORCE
	@mkdir -p $(@D)
	@[ "$$(cat $@ 2>/dev/null)" = "$(PLATFORM)" ] || echo $(PLATFORM) > $@
FORCE:
$(O)/editor: $(O)/libengine.a $(O)/.platform $(EDITOR_SRC) editor/*.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -Iruntime -Ithird_party $(PLAT_INC) $(EDITOR_SRC) $(O)/libengine.a $(PLAT_LIB) -lm -o $@

clean:
	rm -rf $(O)/eng $(O)/libengine.a $(O)/editor $(O)/.platform
