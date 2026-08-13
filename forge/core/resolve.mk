# forge/core/resolve.mk — resolve the product's SELECTION into recipe paths, source paths,
# host-dep lists, and forge.conf. Included by rules.mk after config.mk sets the selection.
# Chip/board-agnostic; maps selection -> where things live. No build targets (that's rules.mk).
#
# Two-root addressing: FORGE_ROOT = forge/ holds the CATALOGS (providers/ packages/
# hostpackages/ steps/); REPO_ROOT = git root holds custom source (kernel/ libc/ …).
# Catalog paths use FORGE_ROOT; PKG_SOURCE uses REPO_ROOT.
FORGE_DIR  := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
FORGE_ROOT := $(abspath $(FORGE_DIR)/..)
REPO_ROOT  := $(abspath $(FORGE_ROOT)/..)

# `override` so the strip also applies to command-line assignments (make PACKAGES=..). It's
# load-bearing: axes are interpolated into providers/<role>/<impl>/ paths, where a stray
# space misroutes resolution.
override KERNEL     := $(strip $(KERNEL))
override BOOTLOADER := $(strip $(BOOTLOADER))
override LIBC       := $(strip $(LIBC))
override PACKAGES   := $(strip $(PACKAGES))
override MEDIA      := $(strip $(MEDIA))
override BOARD      := $(strip $(BOARD))

# KERNEL/BOOTLOADER/LIBC are NOT enum-checked — valid values ARE "whatever recipe dirs exist"
# (validated by the wildcard below; adding a provider is a new dir, no edit here). Only the
# two non-recipe-backed checks remain:
$(if $(PACKAGES),,$(error PACKAGES is empty — a rootfs needs at least one package))
$(if $(filter-out nor sd,$(MEDIA)),$(error MEDIA must be nor|sd (got '$(MEDIA)')))

# --- recipe field reader ------------------------------------------------------
# ONE parser for every recipe in the tree, so it can't disagree with the bash-side reader
# (run-recipe.sh recipe_get) on a value. _field <file> <KEY>: last KEY= wins, strip an inline
# ` # comment`, trim one layer of surrounding quotes (a space-bearing value like
# PKG_MAKE_GOALS="all fel" is quoted for bash-source safety). awk not sed: a literal `#` in
# $(shell sed '…#…') trips Make's own comment scan.
_hash := \#
_field = $(strip $(shell awk -F= '/^$(2)=/{sub(/^$(2)=/,""); sub(/[ \t]*$(_hash).*/,""); v=$$0; sub(/^"/,"",v); sub(/"$$/,"",v)} END{print v}' $(1) 2>/dev/null))
# The callers differ only in where the recipe lives (+ hostpkg's fixed key):
_recipe           = $(FORGE_ROOT)/providers/$(1)/$(2)/recipe.sh
_recipe_get       = $(call _field,$(call _recipe,$(1),$(2)),$(3))
_step_get         = $(call _field,$(FORGE_ROOT)/steps/$(1)/recipe.sh,$(2))
_pkg_get          = $(call _field,$(FORGE_ROOT)/packages/$(1)/recipe.sh,$(2))
_hostpkg_get_host = $(call _field,$(FORGE_ROOT)/hostpackages/$(1)/recipe.sh,PKG_HOST_DEPENDS)
_hostpkg_get      = $(call _field,$(FORGE_ROOT)/hostpackages/$(1)/recipe.sh,$(2))

# --- resolve each selected provider (fail generically on a bad selection) -----
$(if $(wildcard $(call _recipe,kernel,$(KERNEL))),,$(error no kernel recipe for KERNEL=$(KERNEL)))
$(if $(wildcard $(call _recipe,bootloader,$(BOOTLOADER))),,$(error no bootloader recipe for BOOTLOADER=$(BOOTLOADER)))
$(if $(wildcard $(call _recipe,libc,$(LIBC))),,$(error no libc recipe for LIBC=$(LIBC)))
KERNEL_RECIPE     := $(call _recipe,kernel,$(KERNEL))
BOOTLOADER_RECIPE := $(call _recipe,bootloader,$(BOOTLOADER))
LIBC_RECIPE       := $(call _recipe,libc,$(LIBC))
# The selected libc's CC/link contract (PKG_CC/CFLAGS/LDFLAGS + crt/lib), beside its recipe.
# A compile class sources it directly — the engine has no per-libc CC knowledge (each libc
# owns its whole contract; a non-musl libc is a new cc-profile.sh, no engine edit).
LIBC_CC_PROFILE   := $(dir $(LIBC_RECIPE))cc-profile.sh

LIBC_FETCH       := $(call _recipe_get,libc,$(LIBC),PKG_FETCH)
# (host deps aren't resolved into vars here — rules.mk reads each layer's PKG_HOST_DEPENDS
#  inline via _recipe_get/_step_get where it builds the `host-<dep>` prerequisites.)

# --- source paths (driven by the recipe's PKG_FETCH) --------------------------
# local -> the repo-root PKG_SOURCE dir; git -> $(BUILD)/<PKG_GIT_CHECKOUT> (a recipe fact, not
# a hardcoded linux/u-boot here); prebuilt (libc/musl) -> a marker dir with no build.sh.
KERNEL_SRC  := $(strip $(if $(filter local,$(call _recipe_get,kernel,$(KERNEL),PKG_FETCH)),     $(REPO_ROOT)/$(call _recipe_get,kernel,$(KERNEL),PKG_SOURCE),         $(BUILD)/$(call _recipe_get,kernel,$(KERNEL),PKG_GIT_CHECKOUT)))
BOOTLDR_SRC := $(strip $(if $(filter local,$(call _recipe_get,bootloader,$(BOOTLOADER),PKG_FETCH)), $(REPO_ROOT)/$(call _recipe_get,bootloader,$(BOOTLOADER),PKG_SOURCE), $(BUILD)/$(call _recipe_get,bootloader,$(BOOTLOADER),PKG_GIT_CHECKOUT)))
LIBC_SRC    := $(strip \
  $(if $(filter local,$(LIBC_FETCH)),    $(REPO_ROOT)/$(call _recipe_get,libc,$(LIBC),PKG_SOURCE), \
  $(if $(filter prebuilt,$(LIBC_FETCH)), $(BUILD)/musl, \
                                         $(BUILD)/$(LIBC))))

# KERNEL axis value "mainline" is named "linux" in CFG/manifests.
BUILD_KERNEL := $(if $(filter mainline,$(KERNEL)),linux,custom)

# Package existence pre-check: a mistyped PACKAGES entry (busibox) errors here at parse time with
# a clear message, rather than failing obscurely at the pkg-<name> graph node. (PKG_LIBC is NOT
# enforced — it's advisory metadata a package declares about its libc surface; an incompatible
# LIBC×package just fails at build with the real compiler/link errors, which for a from-scratch
# libc ARE the port worklist. A libc-selection typo is caught by the recipe wildcard above.)
define _pkg_exists_check
$(if $(wildcard $(FORGE_ROOT)/packages/$(1)/recipe.sh),,$(error PACKAGES: no package '$(1)' (expected forge/packages/$(1)/recipe.sh)))
endef
$(foreach p,$(PACKAGES),$(eval $(call _pkg_exists_check,$(p))))

# --- board target mapping (declared in board.conf, never inferred) ------------
# board.conf is dual-read (bash-sourced by run-recipe.sh; here Make reads KERNEL_TARGET/ROOTFS_TARGET,
# other keys parse into unused vars, harmless). KERNEL_TARGET is a REQUIRED board fact — the engine
# does not guess it from the directory name; ROOTFS_TARGET defaults to it (they're normally equal).
-include $(PRODUCT_DIR)/boards/$(BOARD)/board.conf
KERNEL_TARGET := $(strip $(KERNEL_TARGET))
ifeq ($(KERNEL_TARGET),)
$(error boards/$(BOARD)/board.conf must set KERNEL_TARGET)
endif
ROOTFS_TARGET := $(strip $(if $(ROOTFS_TARGET),$(ROOTFS_TARGET),$(KERNEL_TARGET)))

# Toolchain-prefix + arch DEFAULTS (ARMv7-A / T113-S3). A board.conf (just -included) may
# override; else these apply. Emitted into forge.conf AND used by the `toolchain` banner, so
# they live here as the single source.
TC_ARCH              := $(strip $(if $(TC_ARCH),$(TC_ARCH),armv7-eabihf))
CROSS_COMPILE        := $(strip $(if $(CROSS_COMPILE),$(CROSS_COMPILE),arm-buildroot-linux-gnueabihf-))
ARCH                 := $(strip $(if $(ARCH),$(ARCH),arm))
ROOTFS_CROSS_COMPILE := $(strip $(if $(ROOTFS_CROSS_COMPILE),$(ROOTFS_CROSS_COMPILE),arm-buildroot-linux-musleabihf-))

# config string for bundle/image names: <bootloader>-<kernel>-<libc>-<pkg>[+<pkg>...].
_space := $(subst ,, )
ROOTFS_TAG := $(LIBC)-$(subst $(_space),+,$(PACKAGES))
CFG := $(BOOTLOADER)-$(BUILD_KERNEL)-$(ROOTFS_TAG)

# The rootfs artifact name is keyed by WHAT IT DEPENDS ON — the rootfs tag + link mode — so two
# selections (musl+busybox vs custom+coreutils) or two linkages never clobber each other in the
# shared build/output/ (Yocto keys its deploy artifacts the same way). The bundle/image step
# still copies this to the canonical `initramfs.cpio.gz` inside the per-CFG bundle dir.
_LINK := $(if $(filter dynamic,$(LINKAGE) $(PKG_LINK)),dynamic,static)
INITRAMFS_IMAGE := initramfs-$(ROOTFS_TAG)-$(_LINK).cpio.gz

# --- forge.conf: the resolved build ENV the shell backends `source` -----------
# bash-source-ONLY (NOT Make-included — run-recipe.sh reads it; nothing `include`s it). It is
# the single source of the build environment every node inherits: the addressing roots, the
# WHOLE build-tree dir layout, the toolchain-prefix + arch DEFAULTS, and the resolved
# selection. run-recipe.sh sources it FIRST (before board.conf, which references $FORGE_DIR/
# $BOARD_DIR from here and may override the pins). Resolved ONCE here, not recomputed per node.
# QUOTING: space-bearing values (PACKAGES) are quoted so bash `source` gets them intact.
# rules.mk writes this (a recipe step, for the 3.82 reason noted there).
define FORGE_CONF_BODY
# forge.conf — resolved by forge/core/resolve.mk; DO NOT EDIT (regenerated every build).
# bash-sourced by run-recipe.sh as the build ENV (roots + dir layout + tool pins + selection).
PRODUCT_DIR=$(PRODUCT_DIR)
BOARD_NAME=$(BOARD)
REPO_ROOT=$(REPO_ROOT)
FORGE_ROOT=$(FORGE_ROOT)
FORGE_DIR=$(FORGE_DIR)
# --- build-tree layout (all under the product's build/, derived once from PRODUCT_DIR) -------
BUILD_DIR=$(BUILD)
DOWNLOAD_DIR=$(BUILD)/downloads
TOOLCHAIN_DIR=$(BUILD)/toolchain
ROOTFS_TOOLCHAIN_DIR=$(BUILD)/toolchain-musl
OUTPUT_DIR=$(BUILD)/output
PYENV_DIR=$(BUILD)/pyenv
HOSTMAKE_DIR=$(BUILD)/hostmake
HOSTTOOLS_DIR=$(BUILD)/hosttools
HOST_PREFIX=$(BUILD)/host
# Uniform taskhash cache (run-recipe.sh): FORGE_STAMPS = per-node <LAYER> stamp (content = the
# last-built taskhash); FORGE_SIGS = per-node <LAYER>.taskhash pointer read by dependents for the
# signature ripple. Cover every recipe (not just host tools), so under build/.forge, not build/host.
FORGE_STAMPS=$(BUILD)/.forge/stamps
FORGE_SIGS=$(BUILD)/.forge/sigs
BOARD_DIR=$(PRODUCT_DIR)/boards/$(BOARD)
OVERLAY_DIR=$(PRODUCT_DIR)/overlay
# --- toolchain-prefix + arch (resolved above: board.conf override else ARMv7-A/T113-S3
#     defaults; a board.conf, bash-sourced AFTER this, may re-override for a different arch) --
TC_ARCH=$(TC_ARCH)
CROSS_COMPILE=$(CROSS_COMPILE)
ARCH=$(ARCH)
ROOTFS_CROSS_COMPILE=$(ROOTFS_CROSS_COMPILE)
MEDIA=$(MEDIA)
# KERNEL holds the ARTIFACT name (BUILD_KERNEL: mainline->linux), which is what the shell
# consumers want (image.sh maps linux->mainline back). The raw axis value isn't needed shell-side.
KERNEL=$(BUILD_KERNEL)
BOOTLOADER=$(BOOTLOADER)
LIBC=$(LIBC)
LIBC_SRC=$(LIBC_SRC)
PACKAGES="$(PACKAGES)"
ROOTFS_TAG=$(ROOTFS_TAG)
CFG=$(CFG)
INITRAMFS_IMAGE=$(INITRAMFS_IMAGE)
KERNEL_TARGET=$(KERNEL_TARGET)
ROOTFS_TARGET=$(ROOTFS_TARGET)
KERNEL_RECIPE=$(KERNEL_RECIPE)
BOOTLOADER_RECIPE=$(BOOTLOADER_RECIPE)
LIBC_RECIPE=$(LIBC_RECIPE)
LIBC_CC_PROFILE=$(LIBC_CC_PROFILE)
endef
export FORGE_CONF_BODY
