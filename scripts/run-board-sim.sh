#!/usr/bin/env bash
# Simulate the whole DE2-115 design (rtl/de2_115/de2_115_top.v) running the demo program, with the switches and keys
# driven as a person would. Needs Icarus Verilog and a RISC-V gcc on PATH.
#   bash scripts/run-board-sim.sh
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=build/core-tmp; mkdir -p "$tmp"
DELAY=20 OUT="$tmp/demo-fast.hex" bash scripts/build-demo.sh > /dev/null
iverilog -g2012 -o "$tmp/board" rtl/slice_unit.v rtl/sliced_alu.v rtl/sliced_regfile_serial.v rtl/rv64i_core.v \
  rtl/de2_115/de2_115_top.v rtl/tb/tb_de2_115.v
vvp "$tmp/board" | grep -E "^(PASS|FAIL|counter|after)"
