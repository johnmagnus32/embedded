# forge/engine/engine.mk — the forge Make engine (paired with run-recipe.sh). A product Makefile
# includes this after config.mk sets PREFERRED_PROVIDER_virtual/* + the board. The engine names NO
# component (no kernel/bootloader/libc/…): it resolves each virtual/<x> to its preferred provider
# recipe, generates one build rule per recipe (node = recipe name), and Make walks the graph.

# ---- roots + inputs ----
PRODUCT_DIR ?= $(CURDIR)
BUILD       := $(PRODUCT_DIR)/build
FORGE_DIR   := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
FORGE_ROOT  := $(abspath $(FORGE_DIR)/..)
FORGE_META  := $(FORGE_ROOT)/meta
REPO_ROOT   := $(abspath $(FORGE_ROOT)/..)
$(foreach v,PACKAGES MEDIA BOARD,$(eval override $(v) := $$(strip $$($(v)))))

# ---- recipe catalog + readers (recipe.sh files are parsed by recipe-scan.sh — see it for the awk) ----
# _field <recipe> <KEY> -> its value; _name <recipe path> -> node name (dir basename);
# _byname <name> -> recipe path; _NODES = one graph node per recipe.
# (No inline comments on the `=` lines below: Make keeps the spaces before a `#`, poisoning the value.)
_ALL_RECIPES := $(wildcard $(PRODUCT_DIR)/recipes-*/*/recipe.sh $(PRODUCT_DIR)/packages/*/recipe.sh $(FORGE_META)/recipes-*/*/recipe.sh)
_scan   := $(FORGE_DIR)/recipe-scan.sh
_field   = $(strip $(shell $(_scan) field $(1) $(2)))
_name    = $(notdir $(patsubst %/,%,$(dir $(1))))
_byname  = $(firstword $(foreach r,$(_ALL_RECIPES),$(if $(filter $(1),$(call _name,$(r))),$(r))))
_NODES  := $(sort $(foreach r,$(_ALL_RECIPES),$(call _name,$(r))))

# ---- provider resolution (Yocto PREFERRED_PROVIDER) --------------------------------------------
# recipe-scan lists who provides what ("virtual/x@name"); config's PREFERRED_PROVIDER_virtual/x names
# the one to use. _provides <virtual/x> -> its provider names; _provider -> the preferred one, VERIFIED
# against that list (its name; path via _providerpath); an unset/bad preference errors at parse (_CHECK).
_PROVIDERS := $(shell $(_scan) providers $(_ALL_RECIPES))
_virtuals  := $(patsubst PREFERRED_PROVIDER_%,%,$(filter PREFERRED_PROVIDER_virtual/%,$(.VARIABLES)))
_provides   = $(patsubst $(1)@%,%,$(filter $(1)@%,$(_PROVIDERS)))
_provider   = $(or $(filter $(strip $(PREFERRED_PROVIDER_$(1))),$(call _provides,$(1))),$(error PREFERRED_PROVIDER_$(1)=$(or $(strip $(PREFERRED_PROVIDER_$(1))),<unset>): not a provider of $(1) (have: $(call _provides,$(1)))))
_providerpath = $(call _byname,$(call _provider,$(1)))
_CHECK := $(foreach v,$(_virtuals),$(call _provider,$(v)))

# ---- board (board.conf supplies KERNEL_TARGET; may override arch/prefix below) ----
-include $(PRODUCT_DIR)/boards/$(BOARD)/board.conf
KERNEL_TARGET := $(strip $(KERNEL_TARGET))
$(if $(KERNEL_TARGET),,$(error boards/$(BOARD)/board.conf must set KERNEL_TARGET))
ROOTFS_TARGET := $(strip $(if $(ROOTFS_TARGET),$(ROOTFS_TARGET),$(KERNEL_TARGET)))

# ---- toolchain scalars (the compiler is cross-cutting — CROSS_COMPILE threads into every compile) ----
TC_ARCH       := $(strip $(if $(TC_ARCH),$(TC_ARCH),armv7-eabihf))
ARCH          := $(strip $(if $(ARCH),$(ARCH),arm))
CROSS_COMPILE := $(strip $(if $(CROSS_COMPILE),$(CROSS_COMPILE),$(call _field,$(call _providerpath,virtual/cross-cc),PKG_HOST_CC_PREFIX)))
$(if $(CROSS_COMPILE),,$(error CROSS_COMPILE empty: $(call _provider,virtual/cross-cc) has no PKG_HOST_CC_PREFIX))

# ---- forge.conf: the resolved env run-recipe.sh sources at every node ----
# Structural + toolchain scalars here; the resolved PROVIDER_<x> paths and the product's own labels
# (KERNEL/LIBC/tags via config.mk's PRODUCT_FORGE_CONF) are appended by the write rule below.
define FORGE_CONF_BODY
PRODUCT_DIR=$(PRODUCT_DIR)
BOARD_NAME=$(BOARD)
REPO_ROOT=$(REPO_ROOT)
FORGE_META=$(FORGE_META)
FORGE_DIR=$(FORGE_DIR)
BUILD_DIR=$(BUILD)
DOWNLOAD_DIR=$(BUILD)/downloads
OUTPUT_DIR=$(BUILD)/output
PYENV_DIR=$(BUILD)/pyenv
HOSTMAKE_DIR=$(BUILD)/hostmake
HOSTTOOLS_DIR=$(BUILD)/hosttools
TOOLCHAIN_DIR=$(BUILD)/$(call _provider,virtual/cross-cc)
LIBC_TC_DIR=$(BUILD)/$(call _provider,virtual/cross-cc-initial)
HOSTTOOLS="as awk basename bash cat cc cp curl cut dirname echo env false find gcc git grep gzip head install ld ln ls mkdir mktemp mv nproc pwd readlink rm rmdir sed sh sha256sum sleep sort tail tar tr true xargs xz"
HOSTTOOLS_NONFATAL="addr2line ar bc bison bzip2 c++filt chmod cmp comm cpio cpp date dd diff du egrep expr fgrep file flex g++ gawk getconf gettext hostname id lz4 lzop m4 makeinfo msgfmt nm objcopy objdump od openssl patch perl pkg-config pod2html pod2man pod2text printf python3 ranlib readelf rsync seq size strings stat swig tee touch uname uniq wc whoami zstd"
ASSUME_PROVIDED="make"
SANITY_REQUIRED="make:3.81 gcc:4.8 python3:3.6 git:1.8"
FORGE_STAMPS=$(BUILD)/.forge/stamps
FORGE_SIGS=$(BUILD)/.forge/sigs
BOARD_DIR=$(PRODUCT_DIR)/boards/$(BOARD)
OVERLAY_DIR=$(PRODUCT_DIR)/overlay
TC_ARCH=$(TC_ARCH)
CROSS_COMPILE=$(CROSS_COMPILE)
ARCH=$(ARCH)
MEDIA=$(MEDIA)
PACKAGES="$(PACKAGES)"
KERNEL_TARGET=$(KERNEL_TARGET)
ROOTFS_TARGET=$(ROOTFS_TARGET)
endef
export FORGE_CONF_BODY

# ---- a node's prerequisites: PKG_DEPENDS + PKG_HOST_DEPENDS + the implied compiler, virtuals resolved ----
_vresolve = $(foreach d,$(1),$(if $(filter virtual/%,$(d)),$(call _provider,$(d)),$(d)))
_ndeps = $(call _field,$(1),PKG_DEPENDS) $(call _field,$(1),PKG_HOST_DEPENDS) $(call _field,$(1),PKG_HOST_DEPENDS_$(MEDIA)) $(if $(and $(filter target,$(call _field,$(1),PKG_CLASS)),$(filter virtual/libc,$(call _field,$(1),PKG_DEPENDS))),virtual/cross-cc)

# ---- how one node is built ----
ENGINE    := $(FORGE_DIR)
BUNDLE    := $(BUILD)/bundles/$(CFG)
BOOTSTRAP := PRODUCT_DIR=$(PRODUCT_DIR)
_barrier    = $(if $(filter make,$(1)),,make)
_noderecipe = $(call _byname,$(1))
_rawdeps    = $(call _ndeps,$(call _noderecipe,$(1)))

# ---- targets ----
.DEFAULT_GOAL := image
.PHONY: clean forge.conf $(BUILD)/forge.conf $(_NODES)
$(BUILD)/forge.conf:
	@mkdir -p $(BUILD)
	@printf '%s\n' "$$FORGE_CONF_BODY" > $@
	@printf '%s\n' $(foreach v,$(_virtuals),'PROVIDER_$(subst -,_,$(patsubst virtual/%,%,$(v)))=$(call _providerpath,$(v))') >> $@
	@printf '%s\n' "$$PRODUCT_FORGE_CONF" >> $@
forge.conf: $(BUILD)/forge.conf
clean: ; rm -rf $(BUILD)

# _rawdeps runs at CALL-time so a recipe's literal ${PACKAGES} lands in the rule text; the $$ defers
# _vresolve to EVAL-time, after Make has expanded that. Do not collapse the two.
define _node_rule
$(1): $(call _barrier,$(1)) $(BUILD)/forge.conf $$(call _vresolve,$(call _rawdeps,$(1)))
	@$(BOOTSTRAP) RECIPE=$(call _noderecipe,$(1)) LAYER=$(1) $(ENGINE)/run-recipe.sh
endef
$(foreach n,$(_NODES),$(eval $(call _node_rule,$(n))))
