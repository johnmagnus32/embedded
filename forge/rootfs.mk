# forge/rootfs.mk — build the SELECTED rootfs (libc + coreutils) into an initramfs.
#   LIBC=custom + COREUTILS=custom (BUILD_ROOTFS=scratch) -> the from-scratch rootfs
#        ASSEMBLER (projects/*/rootfs/Makefile) links our libc + our coreutils.
#   LIBC=musl + COREUTILS=busybox (BUILD_ROOTFS=busybox) -> forge/backends/rootfs.sh
#        (fetch + build static musl BusyBox initramfs).
# Invoked by forge/rules.mk with BUILD_ROOTFS/BACKENDS/PRODUCT_DIR + BACKEND_ENV set.
#
# NOTE: the staging-tree + walk + device-table assembly lives in the product's
# rootfs/Makefile (the "scratch" path) and backends/rootfs.sh (the "busybox" path).
# This backend dispatches to those proven assemblers.
.PHONY: build
ifeq ($(BUILD_ROOTFS),scratch)
build:
	@echo "[forge] rootfs=scratch -> $(PRODUCT_DIR)/rootfs (our libc + coreutils, BOARD=$(ROOTFS_TARGET))"
	@$(MAKE) --no-print-directory -C $(PRODUCT_DIR)/rootfs BOARD=$(ROOTFS_TARGET) rootfs
else
build:
	@echo "[forge] rootfs=busybox -> backends/rootfs.sh"
	@$(BACKEND_ENV) $(BACKENDS)/rootfs.sh
endif
