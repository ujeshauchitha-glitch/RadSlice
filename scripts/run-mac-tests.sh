#!/usr/bin/env bash
# Simulate rtl/mac_array.v against vectors from the integer reference of the digit classifier (G19).
# Needs Icarus Verilog on PATH and python with numpy; results/g19_network.npz comes from scripts/g19_reference.py.
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
for cfg in "8 0" "8 4" "4 2" "10 6"; do
  set -- $cfg
  P=$1; S=$2
  python scripts/g19_mac_vectors.py "$P" 300 > "$tmp/v$P.txt"
  iverilog -g2012 -o "$tmp/tb" -P tb_mac_array.P="$P" -P tb_mac_array.S="$S" rtl/mac_pe.v rtl/mac_array.v rtl/tb/tb_mac_array.v
  vvp "$tmp/tb" +vec="$tmp/v$P.txt" | grep -E "^(PASS|FAIL|MISMATCH)" | tee "$tmp/r.txt"
  grep -q "^PASS" "$tmp/r.txt"
  if [ "$S" -gt 0 ]; then
    vvp "$tmp/tb" +vec="$tmp/v$P.txt" +mutant | grep -E "^(PASS|FAIL)" | tee "$tmp/m.txt"
    grep -q "^PASS" "$tmp/m.txt"
  fi
done
