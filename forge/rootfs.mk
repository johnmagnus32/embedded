# forge/rootfs.mk — build the SELECTED rootfs (libc + coreutils) into an initramfs.
#   LIBC=gv3  + COREUTILS=gv3     (BUILD_ROOTFS=scratch) -> the from-scratch rootfs
#        ASSEMBLER (projects/*/rootfs/Makefile) links our libc + our coreutils.
#   LIBC=musl + COREUTILS=busybox (BUILD_ROOTFS=busybox) -> scripts/03-rootfs.sh
#        (fetch + build static musl BusyBox initramfs).
# Invoked by forge/rules.mk with BUILD_ROOTFS/SCRIPTS/PRODUCT_DIR in the environment.
#
# NOTE: the staging-tree + walk + device-table assembly currently lives in the
# product's rootfs/Makefile (the "scratch" path) and 03-rootfs.sh (the "busybox"
# path). Phase 3 adds the overlay-merge step + unifies them here; for now this
# backend dispatches to the existing proven assemblers to preserve parity.
.PHONY: build
ifeq ($(BUILD_ROOTFS),scratch)
build:
	@echo "[forge] rootfs=scratch -> $(PRODUCT_DIR)/rootfs (our libc + coreutils, BOARD=t113)"
	@$(MAKE) --no-print-directory -C $(PRODUCT_DIR)/rootfs BOARD=t113 rootfs
else
build:
	@echo "[forge] rootfs=busybox -> scripts/03-rootfs.sh"
	@$(SCRIPTS)/03-rootfs.sh
endif
