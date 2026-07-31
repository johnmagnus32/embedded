# forge/providers.mk — resolve a product's provider SELECTION into source paths
# and normalize/validate it. Included by forge/rules.mk after the product's
# config.mk has set KERNEL/BOOTLOADER/LIBC/COREUTILS/BOARD/MEDIA/LCD.
#
# This is the engine's "which implementation for each layer" resolver. It is
# chip- and board-agnostic; it only maps selection -> where the sources live.

# REPO_ROOT: forge/ lives at the repo top level, so one dir up.
FORGE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO_ROOT := $(abspath $(FORGE_DIR)/..)

# --- normalize (strip stray whitespace so ifeq/filter compare cleanly) --------
KERNEL     := $(strip $(KERNEL))
BOOTLOADER := $(strip $(BOOTLOADER))
LIBC       := $(strip $(LIBC))
COREUTILS  := $(strip $(COREUTILS))
MEDIA      := $(strip $(MEDIA))
LCD        := $(strip $(LCD))
BOARD      := $(strip $(BOARD))

# --- validate -----------------------------------------------------------------
$(if $(filter-out custom mainline,$(KERNEL)),  $(error KERNEL must be custom|mainline (got '$(KERNEL)')))
$(if $(filter-out custom uboot,$(BOOTLOADER)), $(error BOOTLOADER must be custom|uboot (got '$(BOOTLOADER)')))
$(if $(filter-out custom musl,$(LIBC)),        $(error LIBC must be custom|musl (got '$(LIBC)')))
$(if $(filter-out custom busybox,$(COREUTILS)),$(error COREUTILS must be custom|busybox (got '$(COREUTILS)')))
$(if $(filter-out nor sd,$(MEDIA)),            $(error MEDIA must be nor|sd (got '$(MEDIA)')))
$(if $(LCD),$(if $(filter-out ili9341,$(LCD)), $(error LCD must be empty|ili9341 (got '$(LCD)'))))

# --- resolve each selection to a source path ----------------------------------
# custom providers live at the repo root; open-source references are fetched into
# the product's build/ by the engine's backends (forge/*.mk shell out to
# forge/backends/*.sh — the generic fetch/build recipes).
# NB: $(if) PRESERVES leading whitespace after each comma, so strip each result —
# an unstripped path becomes a stray make goal + mis-parses in `make -C`.
KERNEL_SRC    := $(strip $(if $(filter custom,$(KERNEL)),     $(REPO_ROOT)/kernel,     $(BUILD)/linux))
BOOTLDR_SRC   := $(strip $(if $(filter custom,$(BOOTLOADER)), $(REPO_ROOT)/bootloader, $(BUILD)/u-boot))
LIBC_SRC      := $(strip $(if $(filter custom,$(LIBC)),      $(REPO_ROOT)/libc,       $(BUILD)/musl))
COREUTILS_SRC := $(strip $(if $(filter custom,$(COREUTILS)), $(REPO_ROOT)/coreutils,  $(BUILD)/busybox))

# --- bridge the axes to the backends' arg vocabulary --------------------------
# The engine's backends (forge/backends/*.sh + the provider Makefiles) do the
# fetch/build; forge orchestrates them with real Make deps. Their KERNEL/ROOTFS
# arg vocabulary differs from the independent axes above — translate:
#   * KERNEL: axis "mainline" == backend "linux".
#   * ROOTFS: the backends still model ONE combined rootfs axis; only the two
#     "pure" combos map (custom+custom -> scratch, musl+busybox -> busybox). A
#     MIXED combo (e.g. custom libc + busybox) is not yet buildable — it needs the
#     package-oriented assembler (build coreutils against the SELECTED libc +
#     overlay-merge). Flagged so it errors clearly, not silently builds wrong.
BUILD_KERNEL := $(if $(filter mainline,$(KERNEL)),linux,custom)
ifeq ($(LIBC)$(COREUTILS),customcustom)
  BUILD_ROOTFS := scratch
else ifeq ($(LIBC)$(COREUTILS),muslbusybox)
  BUILD_ROOTFS := busybox
else
  $(error LIBC=$(LIBC) + COREUTILS=$(COREUTILS) is a MIXED rootfs, not yet buildable \
    (needs forge overlay-staging). Use custom+custom or musl+busybox.)
endif

# --- board target mapping (Phase 4: engine is board-agnostic) -----------------
# The board tells the engine which build-target each generic provider builds for
# THIS board — the engine must not bake in "t113". board.mk is KEY=value (also
# bash-sourceable by lib.sh). Fall back to the board name's leading token so a
# board without a board.mk still resolves sensibly.
BOARD_MK := $(PRODUCT_DIR)/board/$(BOARD)/board.mk
-include $(BOARD_MK)
KERNEL_TARGET := $(strip $(if $(KERNEL_TARGET),$(KERNEL_TARGET),$(firstword $(subst -, ,$(BOARD)))))
ROOTFS_TARGET := $(strip $(if $(ROOTFS_TARGET),$(ROOTFS_TARGET),$(KERNEL_TARGET)))

# config string used for bundle/image names
CFG := $(BOOTLOADER)-$(BUILD_KERNEL)-$(BUILD_ROOTFS)$(if $(LCD),-lcd_$(LCD))
