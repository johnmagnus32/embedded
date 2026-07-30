# forge/kernel.mk — build the SELECTED kernel provider.
#   KERNEL=custom  -> the from-scratch kernel provider (repo-root kernel/, BOARD=t113)
#   KERNEL=mainline-> fetch + build mainline Linux (scripts/02-kernel.sh, incl LCD hook)
# Invoked by forge/rules.mk with KERNEL/KERNEL_SRC/LCD/SCRIPTS in the environment.
.PHONY: build
ifeq ($(KERNEL),custom)
build:
	@echo "[forge] kernel=custom -> $(KERNEL_SRC) (BOARD=t113)"
	@$(MAKE) --no-print-directory -C $(KERNEL_SRC) BOARD=t113
else
build:
	@echo "[forge] kernel=mainline -> scripts/02-kernel.sh$(if $(LCD), LCD=$(LCD))"
	@LCD=$(LCD) $(SCRIPTS)/02-kernel.sh
endif
