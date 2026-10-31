#!/usr/bin/env bash
# Run rtl/rv64i_core.v in lockstep with the golden core on the RV64I workloads, for K = 8, 16, 32.
# Needs Icarus Verilog and a RISC-V gcc on PATH, and built radslice_trace and workloads
# (cmake --build build --target radslice_trace; bash scripts/build-workloads.sh).
#   bash scripts/run-core-lockstep.sh [workload ...]      default: chacha20 coremark_like
set -euo pipefail
cd "$(dirname "$0")/.."
OBJCOPY=""
for p in riscv64-unknown-elf- riscv64-elf- riscv64-linux-gnu-; do
  command -v "${p}objcopy" >/dev/null && { OBJCOPY="${p}objcopy"; break; }
done
[ -n "$OBJCOPY" ] || { echo "error: no RISC-V objcopy on PATH"; exit 2; }
TRACE=build/radslice_trace; [ -f "$TRACE.exe" ] && TRACE="$TRACE.exe"
tmp=build/core-tmp; mkdir -p "$tmp"
workloads=("$@"); [ ${#workloads[@]} -gt 0 ] || workloads=(chacha20 coremark_like)
for w in "${workloads[@]}"; do
  elf=build/workloads/$w.elf
  # the OSS CAD Suite puts its own runtime DLLs on PATH; the MinGW-built trace tool needs the MinGW ones first
  PATH="/c/msys64/ucrt64/bin:$PATH" "$TRACE" "$elf" > "$tmp/$w.trace"
  "$OBJCOPY" -O binary "$elf" "$tmp/$w.bin"
  python - "$tmp/$w.bin" "$tmp/$w.hex" <<'PY'
import sys
d = open(sys.argv[1], 'rb').read()
open(sys.argv[2], 'w').write('\n'.join('%02x' % b for b in d) + '\n')
PY
  n=$(wc -l < "$tmp/$w.trace")
  for k in 8 16 32; do
    iverilog -g2012 -o "$tmp/core$k" -P tb_rv64i_core.K="$k" rtl/slice_unit.v rtl/sliced_alu.v \
      rtl/sliced_regfile_serial.v rtl/rv64i_core.v rtl/tb/tb_rv64i_core.v
    echo -n "$w: "
    vvp "$tmp/core$k" +mem="$tmp/$w.hex" +trace="$tmp/$w.trace" +count="$n" 2>&1 | grep -E "^(PASS|FAIL)" | tee "$tmp/result.txt"
    grep -q "^PASS" "$tmp/result.txt"
  done
done
