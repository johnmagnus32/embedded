# providers.mk — resolve the config.mk provider SELECTION into source paths + a
# concrete build invocation.
#
# Phase 0 (this version): providers still live in-project and the actual build is
# still done by scripts/build.sh. This file's job here is (a) validate the
# selection, (b) resolve each provider to its current in-project source path, and
# (c) translate the doc's INDEPENDENT axes (KERNEL/LIBC/COREUTILS/BOOTLOADER) into
# the arguments scripts/build.sh understands today (KERNEL + a combined ROOTFS).
# Later phases replace the build.sh shell-out with forge/*.mk Make rules and move
# the sources to embedded/.

# --- normalize (strip stray whitespace so `ifeq`/`filter` compare cleanly) ---
KERNEL     := $(strip $(KERNEL))
BOOTLOADER := $(strip $(BOOTLOADER))
LIBC       := $(strip $(LIBC))
COREUTILS  := $(strip $(COREUTILS))
MEDIA      := $(strip $(MEDIA))
LCD        := $(strip $(LCD))

# --- validate the selection --------------------------------------------------
$(if $(filter-out custom mainline,$(KERNEL)),    $(error KERNEL must be custom|mainline (got '$(KERNEL)')))
$(if $(filter-out custom uboot,$(BOOTLOADER)),   $(error BOOTLOADER must be custom|uboot (got '$(BOOTLOADER)')))
$(if $(filter-out gv3 musl,$(LIBC)),             $(error LIBC must be gv3|musl (got '$(LIBC)')))
$(if $(filter-out gv3 busybox,$(COREUTILS)),     $(error COREUTILS must be gv3|busybox (got '$(COREUTILS)')))
$(if $(filter-out nor sd,$(MEDIA)),              $(error MEDIA must be nor|sd (got '$(MEDIA)')))
$(if $(filter-out ili9341,$(LCD)),$(if $(LCD),   $(error LCD must be empty|ili9341 (got '$(LCD)'))))

# --- resolve each provider selection to its (current, in-project) source ------
# In Phase 1 these graduate to $(EMBEDDED)/{kernel,bootloader,libc,coreutils};
# for now they point at the in-project locations so nothing has to move yet.
PRODUCT_DIR    := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
KERNEL_SRC     := $(if $(filter custom,$(KERNEL)),     $(PRODUCT_DIR)/kernel,        $(PRODUCT_DIR)/build/linux)
BOOTLDR_SRC    := $(if $(filter custom,$(BOOTLOADER)), $(PRODUCT_DIR)/bootloader,    $(PRODUCT_DIR)/build/u-boot)
LIBC_SRC       := $(if $(filter gv3,$(LIBC)),          $(PRODUCT_DIR)/rootfs/libc,   $(PRODUCT_DIR)/build/musl)
COREUTILS_SRC  := $(if $(filter gv3,$(COREUTILS)),     $(PRODUCT_DIR)/rootfs/bin,    $(PRODUCT_DIR)/build/busybox)

# --- translate to scripts/build.sh's current interface (Phase 0 bridge) -------
# build.sh takes KERNEL={custom,linux}, BOOTLOADER={custom,uboot}, ROOTFS={busybox,scratch}.
#   * KERNEL: doc's "mainline" == build.sh's "linux".
#   * ROOTFS: build.sh has ONE combined rootfs axis, but the doc splits it into
#     LIBC + COREUTILS. Only the two "pure" combinations map cleanly today:
#         LIBC=gv3  + COREUTILS=gv3      -> ROOTFS=scratch  (our libc + our utils)
#         LIBC=musl + COREUTILS=busybox  -> ROOTFS=busybox  (musl + BusyBox)
#     The MIXED combos (gv3+busybox, musl+gv3) are the whole POINT of splitting
#     coreutils into its own provider, but they need forge/rootfs.mk's overlay/
#     staging (Phase 2/3) to build — they can't be expressed via build.sh's single
#     ROOTFS arg. So error clearly now rather than silently building the wrong thing.
BUILD_KERNEL := $(if $(filter mainline,$(KERNEL)),linux,custom)

ifeq ($(LIBC)$(COREUTILS),gv3gv3)
  BUILD_ROOTFS := scratch
else ifeq ($(LIBC)$(COREUTILS),muslbusybox)
  BUILD_ROOTFS := busybox
else
  $(error LIBC=$(LIBC) + COREUTILS=$(COREUTILS) is a MIXED rootfs, not yet buildable \
    via scripts/build.sh (needs forge/rootfs.mk overlay staging — Phase 2/3). \
    Use gv3+gv3 or musl+busybox for now.)
endif
