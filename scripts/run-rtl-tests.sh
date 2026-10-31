#!/usr/bin/env bash
# Simulate rtl/sliced_alu.v against vectors from the C++ model (which is checked against the golden core).
# Needs Icarus Verilog on PATH (the OSS CAD Suite has it) and a built radslice_alu_vectors.
#   cmake --build build --target radslice_alu_vectors && bash scripts/run-rtl-tests.sh
set -euo pipefail
cd "$(dirname "$0")/.."
VEC=build/radslice_alu_vectors
[ -x "$VEC" ] || VEC=build/radslice_alu_vectors.exe
tmp=$(mktemp -d)
for k in 8 16 32; do
  "$VEC" "$k" 20000 1 > "$tmp/v$k.txt"
  iverilog -g2012 -o "$tmp/tb$k" -P tb_sliced_alu.K="$k" rtl/slice_unit.v rtl/sliced_alu.v rtl/tb/tb_sliced_alu.v
  vvp "$tmp/tb$k" +vec="$tmp/v$k.txt" | grep -E "^(PASS|FAIL)" | tee "$tmp/r$k.txt"
  grep -q "^PASS" "$tmp/r$k.txt"
done
RFV=build/radslice_rf_vectors
[ -x "$RFV" ] || RFV=build/radslice_rf_vectors.exe
for sp in 0 8 32; do
  "$RFV" "$sp" 60000 7 > "$tmp/rf$sp.txt"
  iverilog -g2012 -o "$tmp/rftb$sp" -P tb_sliced_regfile.SPARES="$sp" rtl/sliced_regfile.v rtl/tb/tb_sliced_regfile.v
  vvp "$tmp/rftb$sp" +vec="$tmp/rf$sp.txt" | grep -E "^(PASS|FAIL)" | tee "$tmp/rr$sp.txt"
  grep -q "^PASS" "$tmp/rr$sp.txt"
done
