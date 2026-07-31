# forge/bootloader.mk — build the SELECTED bootloader provider.
#   BOOTLOADER=custom -> the from-scratch bootloader (repo-root bootloader/):
#                        the SD-boot eGON + the FEL-loadable image (0x28000).
#   BOOTLOADER=uboot  -> fetch + build mainline U-Boot (forge/backends/uboot.sh).
# Invoked by forge/rules.mk with BOOTLOADER/BOOTLDR_SRC/BACKENDS + BACKEND_ENV set.
.PHONY: build
ifeq ($(BOOTLOADER),custom)
build:
	@echo "[forge] bootloader=custom -> $(BOOTLDR_SRC) (+ FEL image @0x28000)"
	@$(MAKE) --no-print-directory -C $(BOOTLDR_SRC)
	@$(MAKE) --no-print-directory -C $(BOOTLDR_SRC) fel
else
build:
	@echo "[forge] bootloader=uboot -> backends/uboot.sh"
	@$(BACKEND_ENV) $(BACKENDS)/uboot.sh
endif
