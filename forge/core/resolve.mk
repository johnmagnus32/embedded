# forge/core/resolve.mk — resolve the product's SELECTION into recipe paths, source paths,
# host-dep lists, and forge.conf. Included by rules.mk after config.mk sets the selection.
# Chip/board-agnostic; maps selection -> where things live. No build targets (that's rules.mk).
#
# Two-root addressing: FORGE_ROOT = forge/ holds the recipe CATALOG (recipes-<domain>/, Yocto-style);
# the PRODUCT is its own layer (its recipes-*/ + packages/). REPO_ROOT = git root holds custom source
# (kernel/ libc/ …). Recipes are found by name/virtual (see below); PKG_SOURCE uses REPO_ROOT.
FORGE_DIR  := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
FORGE_ROOT := $(abspath $(FORGE_DIR)/..)
REPO_ROOT  := $(abspath $(FORGE_ROOT)/..)

# `override` so the strip also applies to command-line assignments (make PACKAGES=..). It's
# load-bearing: axis values are matched against each recipe's PKG_ALIAS, where a stray space
# misroutes resolution.
override KERNEL     := $(strip $(KERNEL))
override BOOTLOADER := $(strip $(BOOTLOADER))
override LIBC       := $(strip $(LIBC))
override INIT       := $(strip $(INIT))
override PACKAGES   := $(strip $(PACKAGES))
override MEDIA      := $(strip $(MEDIA))
override BOARD      := $(strip $(BOARD))

# KERNEL/BOOTLOADER/LIBC are NOT enum-checked — valid values ARE "whatever recipe dirs exist"
# (validated by the wildcard below; adding a provider is a new dir, no edit here). Only the
# two non-recipe-backed checks remain:
$(if $(PACKAGES),,$(error PACKAGES is empty — a rootfs needs at least one package))
$(if $(filter-out nor sd,$(MEDIA)),$(error MEDIA must be nor|sd (got '$(MEDIA)')))

# --- toolchain axis: HOW the from-scratch libc's compiler is delivered --------
# prebuilt = the Bootlin cross toolchain (our libc rides it bare via -nostdlib); source = build
# GCC + binutils from source (toolchain-gcc) with our libc as its real sysroot (a NORMAL
# cross-link). Enum-checked (a MODE, not a provider dir). ORTHOGONAL to LIBC: it does NOT rewrite
# LIBC — LIBC stays exactly what the user selected. TOOLCHAIN instead drives (a) the rootfs cross
# toolchain below, (b) the from-scratch libc's cc-profile/stage-runtime (which branch on $TOOLCHAIN),
# and (c) a build-skip in the libc class (source mode builds the libc inside the toolchain). All
# three read $TOOLCHAIN from forge.conf, and (c) the SPLIT toolchain graph: the libc is built with the
# stage-1 LIBC_TC (toolchain-gcc-initial) while packages compile with the stage-2 ROOTFS_TC (toolchain-gcc,
# built --with-sysroot=the libc). Only meaningful for LIBC=custom; a no-op for musl.
override TOOLCHAIN := $(strip $(if $(TOOLCHAIN),$(TOOLCHAIN),gcc))
$(if $(filter-out gcc custom,$(TOOLCHAIN)),$(error TOOLCHAIN must be gcc|custom (got '$(TOOLCHAIN)')))
# Everything is FROM SOURCE now (no prebuilt downloads). Two toolchains:
#   gcc    = from-source GCC/binutils; builds ANY libc (musl or our custom) into its --with-sysroot.
#   custom = our own cpp/cc/as/ar/ld (toolchain-custom + forge-cc driver); only the custom libc — our cc
#            is a C subset that can't build musl (nor the kernel/U-Boot yet).
$(if $(and $(filter musl,$(LIBC)),$(filter custom,$(TOOLCHAIN))),$(error LIBC=musl requires TOOLCHAIN=gcc — the custom cc cannot build musl))
_GCC_TC    := $(filter gcc,$(TOOLCHAIN))
_CUSTOM_TC := $(filter custom,$(TOOLCHAIN))

# --- recipe catalog + field reader --------------------------------------------
# Yocto-style FLAT catalog: recipes live in recipes-<domain>/<name>/recipe.sh under the forge tree AND
# the product (the product is its own layer). A recipe declares its ROLE via METADATA, not its directory:
#   PKG_CLASS   target | native | cross | image   (native/cross => a host tool; see rules.mk enumeration)
#   PKG_PROVIDES virtual/<axis>   + PKG_ALIAS <value>   for a swappable axis (Yocto virtual/* + PREFERRED_PROVIDER)
# So the engine finds a recipe by NAME (host tools, packages, steps) or by (virtual, alias) (kernel/libc/…),
# never by a hardcoded path. Adding an implementation = a new recipe dir with metadata; no edit here.
_ALL_RECIPES := $(wildcard $(PRODUCT_DIR)/recipes-*/*/recipe.sh $(PRODUCT_DIR)/packages/*/recipe.sh $(FORGE_ROOT)/recipes-*/*/recipe.sh)

# ONE field reader (must agree with run-recipe.sh recipe_get): last KEY= wins, strip inline `# comment`,
# trim one quote layer. awk not sed: a literal `#` in $(shell sed '…#…') trips Make's own comment scan.
_hash := \#
_field = $(strip $(shell awk -F= '/^$(2)=/{sub(/^$(2)=/,""); sub(/[ \t]*$(_hash).*/,""); v=$$0; sub(/^"/,"",v); sub(/"$$/,"",v)} END{print v}' $(1) 2>/dev/null))

# _byname(name) -> recipe path whose dir basename == name (product layer wins over forge). For anything
# referenced by NAME: host tools (PKG_HOST_DEPENDS / ROOTFS_TC / LIBC_TC), packages, steps.
_byname = $(firstword $(foreach r,$(_ALL_RECIPES),$(if $(filter $(1),$(notdir $(patsubst %/,%,$(dir $(r))))),$(r))))

# virtual/* index (built once, one awk per recipe): "virtual/<axis>@<alias>@<path>" per provider recipe.
# _prov(virtual, alias) -> the provider recipe path (Yocto's PREFERRED_PROVIDER_virtual/<axis>).
_PROV_INDEX := $(shell for r in $(_ALL_RECIPES); do \
  pa=$$(awk -F= '/^PKG_PROVIDES=/{v=$$2} /^PKG_ALIAS=/{a=$$2} END{if(v!=""){gsub(/[ \t$(_hash)].*/,"",v);gsub(/[ \t$(_hash)].*/,"",a);print v"@"a}}' "$$r"); \
  [ -n "$$pa" ] && echo "$$pa@$$r"; done)
_prov = $(patsubst $(1)@$(2)@%,%,$(filter $(1)@$(2)@%,$(_PROV_INDEX)))

# Toolchain VIRTUALs (Yocto's virtual/cross-cc): a recipe depends on the abstract compiler, not a
# concrete toolchain name, and the engine resolves it. This _V_<virtual> table is forge's PREFERRED_
# PROVIDER_virtual/* — the ONE place a virtual maps to a concrete provider name (recursive `=` so the
# toolchain vars it references, resolved further below, are in scope). `virtual/cross-cc` = the rootfs
# userspace compiler; libc's build toolchain (LIBC_TC) + the kernel/U-Boot toolchain are separate slots
# for now (unified onto one virtual/cross-cc later, when one toolchain builds everything). _vresolve
# rewrites the virtual tokens in a dep list to concrete names; a non-virtual token passes through.
_V_virtual/cross-cc = $(ROOTFS_TC)
_vresolve = $(foreach d,$(1),$(or $(_V_$(d)),$(d)))
# _ndeps(recipe) -> every node-name a recipe depends on: its declared PKG_DEPENDS + PKG_HOST_DEPENDS
# [+ the per-MEDIA arm], PLUS the IMPLIED toolchain — a recipe that links libc is built by
# virtual/cross-cc, so the compiler edge is DERIVED from PKG_DEPENDS=libc, not written in each recipe
# (Bitbake couples DEPENDS-on-libc to the target compiler the same way). Kept in sync with the bash side
# in run-recipe.sh compute_taskhash. `_vresolve` then rewrites the virtual token to the concrete node.
_ndeps = $(call _field,$(1),PKG_DEPENDS) $(call _field,$(1),PKG_HOST_DEPENDS) $(call _field,$(1),PKG_HOST_DEPENDS_$(MEDIA)) $(if $(and $(filter target,$(call _field,$(1),PKG_CLASS)),$(filter libc,$(call _field,$(1),PKG_DEPENDS))),virtual/cross-cc)

# "<name>|<class>" for every recipe (one scan) — the class REPLACES the old provider/hostpackage/step
# directory as the role signal. rules.mk derives its node sets from this: native|cross => a host tool
# (a `host-<name>` node); aggregate => a packagegroup-style node that only pulls a set of deps.
_CLASS_INDEX := $(shell for r in $(_ALL_RECIPES); do \
  c=$$(awk -F= '/^PKG_CLASS=/{c=$$2} END{gsub(/[ \t$(_hash)].*/,"",c);print c}' "$$r"); \
  echo "$$(basename "$$(dirname "$$r")")|$$c"; done)
_HOST_RECIPE_NAMES := $(sort $(patsubst %|native,%,$(filter %|native,$(_CLASS_INDEX))) $(patsubst %|cross,%,$(filter %|cross,$(_CLASS_INDEX))))
_AGG_RECIPE_NAMES  := $(sort $(patsubst %|aggregate,%,$(filter %|aggregate,$(_CLASS_INDEX))))

# Compatibility getters used across the engine, now backed by name/virtual resolution (no dir paths):
_recipe_get       = $(call _field,$(call _prov,virtual/$(1),$(2)),$(3))   # (axis, aliasvalue, key)
_step_get         = $(call _field,$(call _byname,$(1)),$(2))
_pkg_recipe       = $(call _byname,$(1))
_pkg_get          = $(call _field,$(call _pkg_recipe,$(1)),$(2))
_hostpkg_get_host = $(call _field,$(call _byname,$(1)),PKG_HOST_DEPENDS)
_hostpkg_get      = $(call _field,$(call _byname,$(1)),$(2))

# --- resolve each selected provider via its virtual/* (fail on a bad selection) -----
$(if $(call _prov,virtual/kernel,$(KERNEL)),,$(error no recipe provides virtual/kernel for KERNEL=$(KERNEL)))
$(if $(call _prov,virtual/bootloader,$(BOOTLOADER)),,$(error no recipe provides virtual/bootloader for BOOTLOADER=$(BOOTLOADER)))
$(if $(call _prov,virtual/libc,$(LIBC)),,$(error no recipe provides virtual/libc for LIBC=$(LIBC)))
$(if $(call _prov,virtual/init,$(INIT)),,$(error no recipe provides virtual/init for INIT=$(INIT)))
KERNEL_RECIPE     := $(call _prov,virtual/kernel,$(KERNEL))
BOOTLOADER_RECIPE := $(call _prov,virtual/bootloader,$(BOOTLOADER))
LIBC_RECIPE       := $(call _prov,virtual/libc,$(LIBC))
INIT_RECIPE       := $(call _prov,virtual/init,$(INIT))
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
# a clear message, rather than failing obscurely at the pkg-<name> graph node. libc compatibility
# is NOT pre-checked — an incompatible LIBC×package just fails at build with the real compiler/link
# errors, which for a from-scratch libc ARE the port worklist. (A libc-selection typo is caught by
# the recipe wildcard above.)
define _pkg_exists_check
$(if $(call _pkg_recipe,$(1)),,$(error PACKAGES: no recipe named '$(1)' in any recipes-*/ (product or forge) or $(PRODUCT_DIR)/packages/))
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
ARCH                 := $(strip $(if $(ARCH),$(ARCH),arm))
# ONE toolchain builds EVERYTHING (kernel/U-Boot + libc + rootfs), chosen by (LIBC, TOOLCHAIN): the
# from-scratch libc + TOOLCHAIN=source uses the from-source toolchain-gcc (arm-forge triple, our libc as
# its sysroot); TOOLCHAIN=custom uses our own tools; everything else the prebuilt Bootlin musl toolchain.
# ROOTFS_TC (the host-package NAME) is the single source rules.mk + the libc recipe read.
ROOTFS_TC            := $(if $(_GCC_TC),toolchain-gcc,toolchain-custom)
ROOTFS_CROSS_COMPILE := $(strip $(if $(ROOTFS_CROSS_COMPILE),$(ROOTFS_CROSS_COMPILE),$(if $(_GCC_TC),arm-forge-linux-gnueabihf-,arm-forge-custom-)))
# kernel/U-Boot are -ffreestanding, so the libc the toolchain targets is irrelevant to them — they build
# with the SAME toolchain as the rootfs. CROSS_COMPILE (the name kbuild/make-c expect) is just its alias
# (a board/CLI may still override for a genuinely different kernel toolchain).
CROSS_COMPILE        := $(strip $(if $(CROSS_COMPILE),$(CROSS_COMPILE),$(ROOTFS_CROSS_COMPILE)))
# LIBC_TC — the toolchain the LIBC NODE is built WITH (vs ROOTFS_TC = what PACKAGES compile with). They
# DIVERGE only for the from-source stack: the libc builds with the stage-1 toolchain-gcc-initial, then
# the stage-2 toolchain-gcc (ROOTFS_TC) is built --with-sysroot=the libc. Everywhere else LIBC_TC ==
# ROOTFS_TC (the prebuilt musl toolchain), so the split is inert. The libc recipe folds LIBC_TC via
# PKG_HOST_DEPENDS; rules.mk makes `libc` depend on host-$(LIBC_TC). LIBC_TC_DIR is where build-sysroot.sh
# finds the stage-1 gcc (by full path — PATH carries ROOTFS_TC for packages, so the libc build can't rely on it).
LIBC_TC              := $(if $(_GCC_TC),toolchain-gcc-initial,$(ROOTFS_TC))
LIBC_TC_DIR          := $(BUILD)/$(LIBC_TC)

# config string for bundle/image names: <bootloader>-<kernel>-<libc>-<init>-<pkg>[+<pkg>...].
# INIT is part of the rootfs identity (its /init + init config), so it's in the tag — otherwise
# INIT=custom and INIT=runit (same libc/pkgs) would clobber the same initramfs/bundle name.
_space := $(subst ,, )
# The from-source / from-scratch toolchain builds get a `-src` / `-cust` libc tag so their artifacts
# don't clobber the prebuilt-toolchain build of the same libc (all are LIBC=custom; only the TOOLCHAIN differs).
# musl has only one toolchain (gcc), so its tag stays `musl`; the custom libc distinguishes its
# toolchain (custom-gcc vs custom-custom) so the two don't clobber each other's artifacts.
_LIBC_TAG := $(if $(filter custom,$(LIBC)),$(LIBC)-$(TOOLCHAIN),$(LIBC))
ROOTFS_TAG := $(_LIBC_TAG)-$(INIT)-$(subst $(_space),+,$(PACKAGES))
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
TOOLCHAIN_DIR=$(BUILD)/$(ROOTFS_TC)
ROOTFS_TOOLCHAIN_DIR=$(BUILD)/$(ROOTFS_TC)
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
# TOOLCHAIN axis + the resolved toolchains: the from-scratch libc's cc-profile.sh / stage-runtime.sh
# branch on TOOLCHAIN. ROOTFS_TC = what packages compile with (stage-2 toolchain-gcc for source, else
# musl); LIBC_TC = what the libc node builds with (stage-1 toolchain-gcc-initial for source, else musl);
# LIBC_TC_DIR = its dir (build-sysroot.sh runs the stage-1 gcc from there by full path).
TOOLCHAIN=$(TOOLCHAIN)
ROOTFS_TC=$(ROOTFS_TC)
LIBC_TC=$(LIBC_TC)
LIBC_TC_DIR=$(LIBC_TC_DIR)
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
INIT=$(INIT)
INIT_RECIPE=$(INIT_RECIPE)
endef
export FORGE_CONF_BODY
