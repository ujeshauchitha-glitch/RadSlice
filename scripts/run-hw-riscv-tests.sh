#!/usr/bin/env bash
# Run riscv-tests on the golden core with the RadSlice hardware model installed.
#
#   scripts/run-hw-riscv-tests.sh <K> [suite ...]      K = 8, 16 or 32 (sliced ALU width)
#
# Uses riscv-emu's own run-riscv-tests.sh with EMU pointed at radslice_run, so the
# tests are built exactly as for the golden model. Needs a RISC-V toolchain on PATH
# and a built build/radslice_run. Environment: RADSLICE_ALU, RADSLICE_RF (see
# src/radslice_run.cpp).
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
K="${1:?usage: $0 <K> [suite ...]}"; shift
SUITES="${*:-rv64ui rv64mi}"

EMU=""
for c in "$ROOT/build/radslice_run" "$ROOT/build/radslice_run.exe"; do
  [ -x "$c" ] && EMU="$c" && break
done
[ -n "$EMU" ] || { echo "error: build radslice_run first (cmake --build build)"; exit 2; }

cd "$ROOT/extern/riscv-emu"
EMU="$EMU" RADSLICE_K="$K" bash scripts/run-riscv-tests.sh $SUITES
