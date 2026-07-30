# board.mk — t113-gameboy → provider build-target mapping.
#
# The forge engine (tier 1) is board-agnostic: it must not bake in "t113". This
# file is the board input that tells the engine WHICH build-target each generic
# provider should build for this board. Phase 4 of the refactor removed the
# hardcoded `BOARD=t113` from forge/*.mk + build.sh; the value now comes here.
#
# Readable by BOTH Make (forge/providers.mk `include`s it) and bash (env.sh
# `source`s it), so: strict KEY=value only — NO `:=`/`?=`, NO inline `#`
# comments (a trailing comment would leak into the value or break bash).
#
# KERNEL_TARGET  the custom kernel provider's board target (kernel/ `BOARD=`);
#                selects board.h's address set (t113 | virt).
# ROOTFS_TARGET  the rootfs assembler's arch target (rootfs/ `BOARD=`): t113
#                allows VFP; virt is VFP-free for QEMU. Normally == KERNEL_TARGET.
KERNEL_TARGET=t113
ROOTFS_TARGET=t113
