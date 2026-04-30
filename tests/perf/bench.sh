#!/bin/bash
# MIPS benchmark — runs sim-core in headless bench mode
CYCLES=${1:-600000000}
DIR="$(cd "$(dirname "$0")" && pwd)"
SIM="$DIR/../../sim/build/sim-core"
FW="$DIR/../../projects/gameboy/build/gameboy.elf"

echo "=== MIPS Benchmark ($CYCLES ticks) ==="
$SIM --machine gameboy --firmware "$FW" --bench "$CYCLES" --no-chardev 2>&1
