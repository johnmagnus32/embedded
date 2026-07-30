# forge/image.mk — assemble the built components into the product's MEDIA output.
#   MEDIA=nor -> a flash bundle dir (components + FEL loader + manifest.env)
#   MEDIA=sd  -> a dd-able full-disk .img (bootloader @8KiB + FAT boot partition)
#
# PARITY NOTE: the media-assembly logic (bundle manifest, GV3NOR1 offsets, SD
# partition + FAT + boot.scr) is non-trivial and already verified in scripts/
# build.sh (emit_bundle / emit_sd_img). Rather than duplicate that path-resolution
# + assembly here and risk drift, this backend invokes build.sh with the resolved
# BACKEND_ENV. build.sh re-checks the (already-built) components and assembles.
# genimage was considered for the SD path (doc Phase 3) but is NOT installed on
# this host, so we keep the proven dd/sfdisk assembly. If build.sh is later fully
# absorbed into Make, this is the single place that changes.
.PHONY: assemble
assemble:
	@echo "[forge] assemble MEDIA=$(MEDIA) ($(CFG)) via build.sh"
	@$(BACKEND_ENV) $(SCRIPTS)/build.sh
