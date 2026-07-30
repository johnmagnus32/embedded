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
$(if $(filter-out gv3 musl,$(LIBC)),           $(error LIBC must be gv3|musl (got '$(LIBC)')))
$(if $(filter-out gv3 busybox,$(COREUTILS)),   $(error COREUTILS must be gv3|busybox (got '$(COREUTILS)')))
$(if $(filter-out nor sd,$(MEDIA)),            $(error MEDIA must be nor|sd (got '$(MEDIA)')))
$(if $(LCD),$(if $(filter-out ili9341,$(LCD)), $(error LCD must be empty|ili9341 (got '$(LCD)'))))

# --- resolve each selection to a source path ----------------------------------
# custom providers live at the repo root; open-source references are fetched into
# the product's build/ by the fetch backends (forge/*.mk shell out to 0N-*.sh).
# NB: $(if) PRESERVES leading whitespace after each comma, so strip each result —
# an unstripped path becomes a stray make goal + mis-parses in `make -C`.
KERNEL_SRC    := $(strip $(if $(filter custom,$(KERNEL)),     $(REPO_ROOT)/kernel,     $(BUILD)/linux))
BOOTLDR_SRC   := $(strip $(if $(filter custom,$(BOOTLOADER)), $(REPO_ROOT)/bootloader, $(BUILD)/u-boot))
LIBC_SRC      := $(strip $(if $(filter gv3,$(LIBC)),          $(REPO_ROOT)/libc,       $(BUILD)/musl))
COREUTILS_SRC := $(strip $(if $(filter gv3,$(COREUTILS)),     $(REPO_ROOT)/coreutils,  $(BUILD)/busybox))

# --- bridge to the current shell backends (build.sh interface) ----------------
# The reproducible fetch/build backends (0N-*.sh + provider Makefiles) still do
# the heavy lifting; forge orchestrates them with real Make deps. Their KERNEL/
# ROOTFS arg vocabulary differs from the doc's independent axes — translate:
#   * KERNEL: doc "mainline" == backend "linux".
#   * ROOTFS: backends have ONE combined rootfs axis; only the two "pure" combos
#     map (gv3+gv3 -> scratch, musl+busybox -> busybox). Mixed = not yet buildable
#     (needs the overlay-merge assembler; flagged so it errors clearly, not wrong).
BUILD_KERNEL := $(if $(filter mainline,$(KERNEL)),linux,custom)
ifeq ($(LIBC)$(COREUTILS),gv3gv3)
  BUILD_ROOTFS := scratch
else ifeq ($(LIBC)$(COREUTILS),muslbusybox)
  BUILD_ROOTFS := busybox
else
  $(error LIBC=$(LIBC) + COREUTILS=$(COREUTILS) is a MIXED rootfs, not yet buildable \
    (needs forge overlay-staging). Use gv3+gv3 or musl+busybox.)
endif

# --- board target mapping (Phase 4: engine is board-agnostic) -----------------
# The board tells the engine which build-target each generic provider builds for
# THIS board — the engine must not bake in "t113". board.mk is KEY=value (also
# bash-sourceable by env.sh). Fall back to the board name's leading token so a
# board without a board.mk still resolves sensibly.
BOARD_MK := $(PRODUCT_DIR)/board/$(BOARD)/board.mk
-include $(BOARD_MK)
KERNEL_TARGET := $(strip $(if $(KERNEL_TARGET),$(KERNEL_TARGET),$(firstword $(subst -, ,$(BOARD)))))
ROOTFS_TARGET := $(strip $(if $(ROOTFS_TARGET),$(ROOTFS_TARGET),$(KERNEL_TARGET)))

# config string used for bundle/image names
CFG := $(BOOTLOADER)-$(BUILD_KERNEL)-$(BUILD_ROOTFS)$(if $(LCD),-lcd_$(LCD))
