# Variant (not applied): thin stack on the rev A board

The enclosure re-derived for the 2090's header re-soldered strip-on-top plus 6 mm J3/J4 sockets: 20.0 mm overall, a
removable GBA-style battery door (2.2 mm bump), D-pad hole 1.05, every stack height driven by `SOCKET_H` /
`HEADER_FLIPPED` / `HEADER_JOINT` in params.py, and stricter checks (context-vs-context interference). It passed every
check (2026-10-14) but needs rework on the ordered rev A boards, so it was not adopted; it is kept as the starting point
for rev B's parametric stack and battery door.

The patch is against the enclosure as of commit ff492e4 (later edits, e.g. DESIGN.md, will conflict): check that
version out (`git worktree add --detach /tmp/v ff492e4`), `git apply` the patch there, copy `battery_door.py` next to
enclosure.py, then regenerate `build/board.json` with KiCad's python (`export_board.py`).
