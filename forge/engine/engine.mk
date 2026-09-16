# forge/engine/engine.mk — the forge Make engine (paired with run-recipe.sh). A product Makefile
# includes this after config.mk sets the selection. Resolves the selection into recipe paths + forge.conf,
# then generates one build rule per node; Make is the only dependency walker.

# ---- roots + inputs ----
PRODUCT_DIR ?= $(CURDIR)
BUILD       := $(PRODUCT_DIR)/build
FORGE_DIR   := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
FORGE_ROOT  := $(abspath $(FORGE_DIR)/..)
FORGE_META  := $(FORGE_ROOT)/meta
REPO_ROOT   := $(abspath $(FORGE_ROOT)/..)
$(foreach v,KERNEL BOOTLOADER LIBC INIT TOOLCHAIN PACKAGES MEDIA BOARD,$(eval override $(v) := $$(strip $$($(v)))))

# ---- recipe catalog + readers (recipe.sh files are parsed by recipe-scan.sh — see it for the awk) ----
_ALL_RECIPES := $(wildcard $(PRODUCT_DIR)/recipes-*/*/recipe.sh $(PRODUCT_DIR)/packages/*/recipe.sh $(FORGE_META)/recipes-*/*/recipe.sh)
_scan        := $(FORGE_DIR)/recipe-scan.sh
_field        = $(strip $(shell $(_scan) field $(1) $(2)))
_byname       = $(firstword $(foreach r,$(_ALL_RECIPES),$(if $(filter $(1),$(notdir $(patsubst %/,%,$(dir $(r))))),$(r))))
_PROV_INDEX  := $(shell $(_scan) providers $(_ALL_RECIPES))
_prov         = $(patsubst $(1)@$(2)@%,%,$(filter $(1)@$(2)@%,$(_PROV_INDEX)))
_prov_name    = $(notdir $(patsubst %/,%,$(dir $(call _prov,$(1),$(2)))))
_CLASS_INDEX := $(shell $(_scan) classes $(_ALL_RECIPES))
_HOST_RECIPE_NAMES := $(sort $(patsubst %|native,%,$(filter %|native,$(_CLASS_INDEX))) $(patsubst %|cross,%,$(filter %|cross,$(_CLASS_INDEX))))
_AGG_RECIPE_NAMES  := $(sort $(patsubst %|aggregate,%,$(filter %|aggregate,$(_CLASS_INDEX))))

# ---- resolve the selected axes (fail loud on a bad selection) ----
$(if $(call _prov,virtual/kernel,$(KERNEL)),,$(error no recipe provides virtual/kernel for KERNEL=$(KERNEL)))
$(if $(call _prov,virtual/bootloader,$(BOOTLOADER)),,$(error no recipe provides virtual/bootloader for BOOTLOADER=$(BOOTLOADER)))
$(if $(call _prov,virtual/libc,$(LIBC)),,$(error no recipe provides virtual/libc for LIBC=$(LIBC)))
$(if $(call _prov,virtual/init,$(INIT)),,$(error no recipe provides virtual/init for INIT=$(INIT)))
KERNEL_RECIPE     := $(call _prov,virtual/kernel,$(KERNEL))
BOOTLOADER_RECIPE := $(call _prov,virtual/bootloader,$(BOOTLOADER))
LIBC_RECIPE       := $(call _prov,virtual/libc,$(LIBC))
INIT_RECIPE       := $(call _prov,virtual/init,$(INIT))
LIBC_CC_PROFILE   := $(dir $(LIBC_RECIPE))cc-profile.sh

# ---- board (board.conf supplies KERNEL_TARGET; may override arch/prefix below) ----
-include $(PRODUCT_DIR)/boards/$(BOARD)/board.conf
KERNEL_TARGET := $(strip $(KERNEL_TARGET))
$(if $(KERNEL_TARGET),,$(error boards/$(BOARD)/board.conf must set KERNEL_TARGET))
ROOTFS_TARGET := $(strip $(if $(ROOTFS_TARGET),$(ROOTFS_TARGET),$(KERNEL_TARGET)))

# ---- toolchain (ONE toolchain, via virtual/cross-cc[-initial], selected by TOOLCHAIN) ----
TC_ARCH       := $(strip $(if $(TC_ARCH),$(TC_ARCH),armv7-eabihf))
ARCH          := $(strip $(if $(ARCH),$(ARCH),arm))
ROOTFS_TC     := $(call _prov_name,virtual/cross-cc,$(TOOLCHAIN))
$(if $(ROOTFS_TC),,$(error TOOLCHAIN=$(TOOLCHAIN): no recipe provides virtual/cross-cc with PKG_ALIAS=$(TOOLCHAIN)))
CROSS_COMPILE := $(strip $(if $(CROSS_COMPILE),$(CROSS_COMPILE),$(call _field,$(call _prov,virtual/cross-cc,$(TOOLCHAIN)),PKG_HOST_CC_PREFIX)))
$(if $(CROSS_COMPILE),,$(error CROSS_COMPILE empty: the $(ROOTFS_TC) recipe has no PKG_HOST_CC_PREFIX))
LIBC_TC       := $(call _prov_name,virtual/cross-cc-initial,$(TOOLCHAIN))
$(if $(LIBC_TC),,$(error TOOLCHAIN=$(TOOLCHAIN): no recipe provides virtual/cross-cc-initial with PKG_ALIAS=$(TOOLCHAIN)))
LIBC_TC_DIR   := $(BUILD)/$(LIBC_TC)

# ---- derived tags (CFG + artifact names) ----
_space := $(subst ,, )
_LIBC_TAG := $(if $(filter custom,$(LIBC)),$(LIBC)-$(TOOLCHAIN),$(LIBC))
ROOTFS_TAG := $(_LIBC_TAG)-$(INIT)-$(subst $(_space),+,$(PACKAGES))
CFG := $(BOOTLOADER)-$(KERNEL)-$(ROOTFS_TAG)
_LINK := $(if $(filter dynamic,$(LINKAGE) $(PKG_LINK)),dynamic,static)
INITRAMFS_IMAGE := initramfs-$(ROOTFS_TAG)-$(_LINK).cpio.gz

# ---- forge.conf: the resolved env run-recipe.sh sources at every node ----
define FORGE_CONF_BODY
PRODUCT_DIR=$(PRODUCT_DIR)
BOARD_NAME=$(BOARD)
REPO_ROOT=$(REPO_ROOT)
FORGE_META=$(FORGE_META)
FORGE_DIR=$(FORGE_DIR)
BUILD_DIR=$(BUILD)
DOWNLOAD_DIR=$(BUILD)/downloads
TOOLCHAIN_DIR=$(BUILD)/$(ROOTFS_TC)
OUTPUT_DIR=$(BUILD)/output
PYENV_DIR=$(BUILD)/pyenv
HOSTMAKE_DIR=$(BUILD)/hostmake
HOSTTOOLS_DIR=$(BUILD)/hosttools
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
TOOLCHAIN=$(TOOLCHAIN)
ROOTFS_TC=$(ROOTFS_TC)
LIBC_TC=$(LIBC_TC)
LIBC_TC_DIR=$(LIBC_TC_DIR)
MEDIA=$(MEDIA)
KERNEL=$(KERNEL)
BOOTLOADER=$(BOOTLOADER)
LIBC=$(LIBC)
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

# ---- a node's prerequisites: PKG_DEPENDS + PKG_HOST_DEPENDS + the implied compiler, virtuals resolved ----
_V_virtual/cross-cc = $(ROOTFS_TC)
_vresolve = $(foreach d,$(1),$(or $(_V_$(d)),$(d)))
_ndeps = $(call _field,$(1),PKG_DEPENDS) $(call _field,$(1),PKG_HOST_DEPENDS) $(call _field,$(1),PKG_HOST_DEPENDS_$(MEDIA)) $(if $(and $(filter target,$(call _field,$(1),PKG_CLASS)),$(filter libc,$(call _field,$(1),PKG_DEPENDS))),virtual/cross-cc)

# ---- how one node is built ----
ENGINE    := $(FORGE_DIR)
BUNDLE    := $(BUILD)/bundles/$(CFG)
BOOTSTRAP := PRODUCT_DIR=$(PRODUCT_DIR)
define _build_recipe
@$(BOOTSTRAP) RECIPE=$(2) LAYER=$(1) $(ENGINE)/run-recipe.sh
endef

# ---- node namespace: axes + steps + aggregates + packages + host tools ----
_NODE_HOSTPKGS := make
_AXES  := kernel bootloader libc init
_STEPS := rootfs image
_NODES := $(_AXES) $(_STEPS) $(_AGG_RECIPE_NAMES) $(PACKAGES) $(_HOST_RECIPE_NAMES)
_RECIPE_kernel     := $(KERNEL_RECIPE)
_RECIPE_bootloader := $(BOOTLOADER_RECIPE)
_RECIPE_libc       := $(LIBC_RECIPE)
_RECIPE_init       := $(INIT_RECIPE)
_noderecipe = $(or $(_RECIPE_$(1)),$(call _byname,$(1)))
_node  = $(if $(filter $(1),$(_AXES) $(_STEPS) $(_AGG_RECIPE_NAMES)),$(1),$(if $(filter $(1),$(_HOST_RECIPE_NAMES)),host-$(1),pkg-$(1)))
_nodes = $(foreach d,$(1),$(call _node,$(d)))
_barrier  = $(if $(filter $(1),$(_NODE_HOSTPKGS)),,$(call _node,make))
_rawdeps  = $(call _ndeps,$(call _noderecipe,$(1)))
_depnodes = $(call _nodes,$(call _vresolve,$(1)))

# ---- targets ----
.PHONY: image clean forge.conf $(BUILD)/forge.conf $(foreach n,$(_NODES),$(call _node,$(n)))
$(BUILD)/forge.conf:
	@mkdir -p $(BUILD)
	@printf '%s\n' "$$FORGE_CONF_BODY" > $@
forge.conf: $(BUILD)/forge.conf
clean: ; rm -rf $(BUILD)

# _rawdeps runs at CALL-time so a recipe's literal ${LIBC_TC}/${PACKAGES} lands in the rule text; the $$
# defers _depnodes to EVAL-time, after Make has expanded those. Do not collapse the two.
define _node_rule
$(call _node,$(1)): $(call _barrier,$(1)) $(BUILD)/forge.conf $$(call _depnodes,$(call _rawdeps,$(1)))
	$(call _build_recipe,$(1),$(call _noderecipe,$(1)))
endef
$(foreach n,$(sort $(_NODES)),$(eval $(call _node_rule,$(n))))
