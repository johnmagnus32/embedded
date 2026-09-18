# Makefile — gameboy-v3 PRODUCT dispatcher. The product has two build concerns, each its own tree:
#   OS image  -> image/  (the engine layer: conf/local.conf + recipes-*; build/ workspace). `make image`
#                delegates there; build individual recipes with `MACHINE=<m> make -C image <recipe>`.
#   console   -> src/    (the canvas app: host browser sim + the source the OS's `console` package
#                cross-compiles). Host-sim binaries land in build/ (separate from image/build).
#
#   MACHINE=t113 make image   # the shippable hw image
#   make flash                        # build the hw NOR bundle + FEL-flash on the rig
#   make sim GAME=breakout            # build the console (browser backend) + run a game in the browser
#   make clean                        # wipe both build trees
PRODUCT_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO_ROOT    := $(abspath $(PRODUCT_ROOT)/../..)

.PHONY: image flash sim clean

# OS image — delegate to the layer. MACHINE flows through the environment (required there, no default,
# so `make image` alone fails loud with the machine list — same as `make -C image`).
image:
	@$(MAKE) -C $(PRODUCT_ROOT)/image image

# flash — build the NOR bundle + flash + FEL-boot it on the rig. Hardware deploy, so this one names the
# machine: t113 is the sole flashable target (not a default — everything else makes you pass MACHINE).
# Uses local.conf's MEDIA=nor (the default); if you switched MEDIA to sd, dd the image instead of flashing.
flash:
	@MACHINE=t113 $(MAKE) -C $(PRODUCT_ROOT)/image image
	@$(REPO_ROOT)/tools/flash.sh $(PRODUCT_ROOT)/image/build/t113/bundle nor

# sim — build the whole console userspace (compositor + services + games) for the host with the browser
# backend, then run a game in the browser:  make sim GAME=breakout
# (build only, no run: `make -C src CANVAS_BACKEND=web`.)
sim:
	@$(MAKE) -C $(PRODUCT_ROOT)/src CANVAS_BACKEND=web
	@$(PRODUCT_ROOT)/run-sim.sh $(GAME)

# clean — both trees: the OS build (image/build, respects MACHINE) + the host console build (build/).
clean:
	@$(MAKE) -C $(PRODUCT_ROOT)/image clean
	@rm -rf $(PRODUCT_ROOT)/build
