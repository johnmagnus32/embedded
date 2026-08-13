#!/usr/bin/env bash
# classes/devicetree.sh — the "devicetree" CLASS: the DT-overlay MECHANISM shared by the OSS git
# providers (mainline kernel + U-Boot) whose build patches a board .dtsi into the upstream DTS.
# A recipe `inherit devicetree` to get apply_dtsi_overlay in scope, then calls it from do_build after
# restoring the pristine .dts. Separate from kconfig — device-tree source is not kernel config.

# apply_dtsi_overlay <dts> <overlay> <src_board_dir> <dts_dir> — copy a board .dtsi beside the tracked
# .dts, append its #include, verify. Caller restores the .dts pristine first (the append isn't idempotent).
apply_dtsi_overlay() {
  local dts="$1" overlay="$2" src_board="$3" dts_dir="$4"
  local include_line="#include \"${overlay}\""
  cp -f "${src_board}/${overlay}" "${dts_dir}/${overlay}"
  grep -qF "${include_line}" "${dts}" || printf '\n%s\n' "${include_line}" >> "${dts}"
  grep -qF "${include_line}" "${dts}" || { echo "apply_dtsi_overlay: failed to append '${include_line}' to ${dts}" >&2; return 1; }
}
