# forge/core/rules.mk — the build engine: top-level targets + the dependency graph.
# A product Makefile is a thin `include $(REPO_ROOT)/forge/core/rules.mk` after config.mk
# sets KERNEL/BOOTLOADER/LIBC/PACKAGES/BOARD/MEDIA + PRODUCT_DIR.
#
# Every buildable is a recipe run through ONE runner (run-recipe.sh); Make is the ONE
# dependency walker (host + target deps are both Make prerequisites).

PRODUCT_DIR ?= $(CURDIR)
BUILD       := $(PRODUCT_DIR)/build

include $(dir $(lastword $(MAKEFILE_LIST)))resolve.mk

CORE  := $(FORGE_DIR)
BUNDLE := $(BUILD)/bundles/$(CFG)

# forge.conf — the resolved selection, written for the recipe shells to `source`; resolve.mk
# packed it into the exported $$FORGE_CONF_BODY define. MUST be a recipe write, not $(shell):
# GNU Make 3.82 (the product's make) exports a define to recipe shells but NOT to parse-time
# $(shell), which would write an empty file. Always-rewritten so a changed selection
# (make image LIBC=musl) is never stale; nothing keys on its mtime.
.PHONY: image toolchain print-config clean host-clean forge.conf $(BUILD)/forge.conf
$(BUILD)/forge.conf:
	@mkdir -p $(BUILD)
	@printf '%s\n' "$$FORGE_CONF_BODY" > $@
forge.conf: $(BUILD)/forge.conf
	@echo "wrote $(BUILD)/forge.conf"

# BOOTSTRAP: the seed a recipe needs BEFORE it can source forge.conf — PRODUCT_DIR locates the
# build tree (hence forge.conf). Everything else (BOARD, layout, pins) comes from forge.conf.
BOOTSTRAP := PRODUCT_DIR=$(PRODUCT_DIR)

# _build_recipe <label> <recipe> — run one node via run-recipe.sh (sources the recipe, calls
# do_fetch/do_build/do_install by name; no per-node makefile, no type dispatch). Host + target
# deps are Make prerequisites, not provisioned here.
define _build_recipe
@$(BOOTSTRAP) RECIPE=$(2) LAYER=$(1) $(CORE)/run-recipe.sh
endef

# --- host tools as graph nodes ------------------------------------------------
# A host tool is just a recipe (in hostpackages/), built by the SAME run-recipe.sh as every
# other node — no separate host runner. Each is a `host-<name>` node; edges come from its
# recipe's PKG_HOST_DEPENDS (host-genimage: host-libconfuse). A build node lists host deps as
# `host-<dep>` prerequisites; Make resolves the closure, orders, builds-once. run-recipe.sh
# skips ANY node (host or target) when its content taskhash is unchanged and its declared output
# artifact is present — one uniform cache, not a host-only path (see run-recipe.sh compute_taskhash).
# Explicit rules via $(eval) — a `host-%` pattern rule is NOT run for a phony prereq-only target.
_HOSTPKGS := $(notdir $(patsubst %/,%,$(wildcard $(FORGE_ROOT)/hostpackages/*/)))
_hostdeps  = $(addprefix host-,$(1))
.PHONY: $(addprefix host-,$(_HOSTPKGS))

# BASE host set: what every build needs regardless of MEDIA/bootloader (make + both cross
# toolchains + gen_init_cpio). These ARE the toolchain — they build with the system's own
# cc/make and declare no host deps, so their nodes must NOT carry the `toolchain` back-edge
# (that would be `toolchain -> host-make -> toolchain`, a cycle). LAZY tools (genimage for
# MEDIA=sd, binman-venv for U-Boot, libconfuse) are everything else: they may need the base
# set + each other, so their nodes DO depend on `toolchain` and on their PKG_HOST_DEPENDS.
_BASE_HOSTPKGS := make toolchain-glibc toolchain-musl gen_init_cpio
_LAZY_HOSTPKGS := $(filter-out $(_BASE_HOSTPKGS),$(_HOSTPKGS))
define _base_host_rule
host-$(1): $(BUILD)/forge.conf
	$$(call _build_recipe,$(1),$(FORGE_ROOT)/hostpackages/$(1)/recipe.sh)
endef
define _lazy_host_rule
host-$(1): toolchain $$(call _hostdeps,$$(call _hostpkg_get_host,$(1))) $(BUILD)/forge.conf
	$$(call _build_recipe,$(1),$(FORGE_ROOT)/hostpackages/$(1)/recipe.sh)
endef
$(foreach h,$(_BASE_HOSTPKGS),$(eval $(call _base_host_rule,$(h))))
$(foreach h,$(_LAZY_HOSTPKGS),$(eval $(call _lazy_host_rule,$(h))))

.PHONY: kernel bootloader libc rootfs $(addprefix pkg-,$(PACKAGES))

# toolchain: the base host set every build needs. Pure Make now — the base `host-<name>` nodes
# are the prerequisites (they have no inter-deps, so order is free); Make builds each once via
# run-recipe.sh, which taskhash-skips an up-to-date tool. The recipe just prints a summary
# banner (versions read from the recipes). All build nodes depend on `toolchain`.
toolchain: $(addprefix host-,$(_BASE_HOSTPKGS)) $(BUILD)/forge.conf
	@printf '\n\033[1;32m[toolchain] DONE\033[0m\n'
	@_m="$(BUILD)/hostmake/bin/make"; [ -x "$$_m" ] || _m=make; \
	 printf '  Host make  : %s  (%s)\n' \
	   "$$( "$$_m" --version 2>/dev/null | sed -n '1s/.*GNU Make //p' )" \
	   "$$( [ -x "$(BUILD)/hostmake/bin/make" ] && echo 'built locally at build/hostmake' || echo "host's own, >= 4.0" )"
	@printf '  Toolchain  : %s glibc %s  [%s]  (U-Boot + kernel)\n' \
	  "$(TC_ARCH)" "$(call _hostpkg_get,toolchain-glibc,PKG_VERSION)" "$(CROSS_COMPILE)"
	@printf '  Rootfs TC  : %s musl %s  [%s]  (rootfs — smaller static)\n' \
	  "$(TC_ARCH)" "$(call _hostpkg_get,toolchain-musl,PKG_VERSION)" "$(ROOTFS_CROSS_COMPILE)"

# --- providers ----------------------------------------------------------------
kernel: toolchain $(call _hostdeps,$(call _recipe_get,kernel,$(KERNEL),PKG_HOST_DEPENDS)) $(BUILD)/forge.conf
	$(call _build_recipe,kernel,$(KERNEL_RECIPE))
bootloader: toolchain $(call _hostdeps,$(call _recipe_get,bootloader,$(BOOTLOADER),PKG_HOST_DEPENDS)) $(BUILD)/forge.conf
	$(call _build_recipe,bootloader,$(BOOTLOADER_RECIPE))
libc: toolchain $(call _hostdeps,$(call _recipe_get,libc,$(LIBC),PKG_HOST_DEPENDS)) $(BUILD)/forge.conf
	$(call _build_recipe,libc,$(LIBC_RECIPE))

# --- packages: each PACKAGE builds into its OWN per-package dir (build/rootfs/pkgstage/<pkg>) -----
# No shared-tree pre-wipe: a package installs into its own dir — a
# durable, cacheable artifact — and the rootfs step assembles STAGE from the SELECTED packages'
# dirs (Buildroot's per-package model), so a de-selected package's files never linger. The libc
# prerequisite is DERIVED from the recipe's PKG_DEPENDS (not hardcoded) — the SAME fact the taskhash
# ripple reads — so the Make edge and the cache-invalidation edge can't drift: a package that omits
# PKG_DEPENDS=libc gets neither (and fails to build against libc, surfacing the omission), rather than
# silently under-invalidating on a libc/cc-profile change. Same pattern as PKG_HOST_DEPENDS. Explicit
# rule per package via $(eval) (same reason as the host nodes above).
define _pkg_rule
pkg-$(1): toolchain $$(call _pkg_get,$(1),PKG_DEPENDS) $$(call _hostdeps,$$(call _pkg_get,$(1),PKG_HOST_DEPENDS)) $(BUILD)/forge.conf
	$$(call _build_recipe,$(1),$(FORGE_ROOT)/packages/$(1)/recipe.sh)
endef
$(foreach p,$(PACKAGES),$(eval $(call _pkg_rule,$(p))))

# --- rootfs (pack the staging tree) + image (compose the built artifacts) -----
rootfs: $(addprefix pkg-,$(PACKAGES)) $(call _hostdeps,$(call _step_get,rootfs,PKG_HOST_DEPENDS)) $(BUILD)/forge.conf
	$(call _build_recipe,rootfs,$(FORGE_ROOT)/steps/rootfs/recipe.sh)
# image host deps: base + the per-MEDIA arm — PKG_HOST_DEPENDS_sd=genimage (nor adds nothing).
image: kernel bootloader rootfs $(call _hostdeps,$(call _step_get,image,PKG_HOST_DEPENDS) $(call _step_get,image,PKG_HOST_DEPENDS_$(MEDIA))) $(BUILD)/forge.conf
	$(call _build_recipe,image,$(FORGE_ROOT)/steps/image/recipe.sh)

print-config:
	@echo "forge config:"
	@printf '  %-10s = %-8s -> %s\n' KERNEL     "$(KERNEL)"     "$(KERNEL_SRC)"
	@printf '  %-10s = %-8s -> %s\n' BOOTLOADER "$(BOOTLOADER)" "$(BOOTLDR_SRC)"
	@printf '  %-10s = %-8s -> %s (libc)\n' LIBC "$(LIBC)" "$(LIBC_SRC)"
	@printf '  %-10s = %s\n' PACKAGES "$(PACKAGES)"
	@echo "  BOARD=$(BOARD)  MEDIA=$(MEDIA)"
	@echo "  board targets: KERNEL_TARGET=$(KERNEL_TARGET)  ROOTFS_TARGET=$(ROOTFS_TARGET)"
	@echo "  cfg: $(CFG)"

clean: host-clean
	rm -rf $(BUILD)/bundles
	@echo "(provider build/ dirs cleaned by their own makefiles)"

.PHONY: host-clean
host-clean:
	rm -rf $(BUILD)/host $(BUILD)/toolchain $(BUILD)/toolchain-musl $(BUILD)/hostmake $(BUILD)/hosttools $(BUILD)/pyenv
	@echo "host tools removed (re-provision with 'make toolchain' or any build)"
