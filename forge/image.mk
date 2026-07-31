# forge/image.mk — assemble the built components into the product's MEDIA output.
#   MEDIA=nor -> a flash bundle dir (components + FEL loader + manifest.env)
#   MEDIA=sd  -> a dd-able full-disk .img (bootloader @8KiB + FAT boot partition)
#
# PARITY NOTE: the media-assembly logic (bundle manifest, GV3NOR1 offsets, SD
# partition + FAT + boot.scr) is non-trivial and already verified in
# forge/backends/image.sh (emit_bundle / emit_sd_img). Rather than duplicate that
# path-resolution + assembly here and risk drift, this backend invokes image.sh
# with the resolved BACKEND_ENV. image.sh re-checks the (already-built) components
# and assembles.
#
# The SD layout is DECLARED in board/$(BOARD)/image.cfg (genimage format) and its
# geometry lives in board/$(BOARD)/layout.env (SDIMAGE_*). genimage would consume
# image.cfg directly, but it is NOT installed on this host (doc Phase 3 "Genimage
# dependency" sanctions keeping the dd/sfdisk shim in the interim), so build.sh's
# emit_sd_img implements that same layout. If genimage lands, this backend swaps
# to `genimage --config board/$(BOARD)/image.cfg` — the single place that changes.
.PHONY: assemble
assemble:
	@echo "[forge] assemble MEDIA=$(MEDIA) ($(CFG)) via backends/image.sh"
	@$(BACKEND_ENV) $(BACKENDS)/image.sh
