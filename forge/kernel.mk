# forge/kernel.mk — build the SELECTED kernel provider.
#   KERNEL=custom  -> the from-scratch kernel provider (repo-root kernel/), built
#                     for the board's KERNEL_TARGET (from board/*/board.mk).
#   KERNEL=mainline-> fetch + build mainline Linux (scripts/02-kernel.sh, incl LCD hook)
# Invoked by forge/rules.mk with KERNEL/KERNEL_SRC/KERNEL_TARGET/LCD/SCRIPTS set.
.PHONY: build
ifeq ($(KERNEL),custom)
build:
	@echo "[forge] kernel=custom -> $(KERNEL_SRC) (BOARD=$(KERNEL_TARGET))"
	@$(MAKE) --no-print-directory -C $(KERNEL_SRC) BOARD=$(KERNEL_TARGET)
else
build:
	@echo "[forge] kernel=mainline -> scripts/02-kernel.sh$(if $(LCD), LCD=$(LCD))"
	@LCD=$(LCD) $(SCRIPTS)/02-kernel.sh
endif
