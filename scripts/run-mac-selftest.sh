#!/usr/bin/env bash
# G19 step 3: defective chips, boot self-test and remap on rtl/mac_array.v (see rtl/tb/tb_mac_selftest.v).
# Needs Icarus Verilog on PATH (OSS CAD Suite: its bin and lib folders) and python with numpy.
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
for cfg in "8 4 0.25" "8 2 0.15" "4 4 0.30"; do
  set -- $cfg
  P=$1; S=$2; Q=$3
  python scripts/g19_mac_vectors.py "$P" 40 > "$tmp/v.txt"
  (cd scripts && python g19_selftest_vectors.py "$P" "$S" 150 "$Q" 5 2) > "$tmp/c.txt"
  iverilog -g2012 -o "$tmp/tb" -P tb_mac_selftest.P="$P" -P tb_mac_selftest.S="$S" rtl/mac_pe.v rtl/mac_array.v rtl/tb/tb_mac_selftest.v
  vvp "$tmp/tb" +vec="$tmp/v.txt" +chips="$tmp/c.txt" | grep -E "^(PASS|FAIL|RAW|RUN|FLIP|DECISION)" | tee "$tmp/r.txt"
  grep -q "^PASS" "$tmp/r.txt"
done
