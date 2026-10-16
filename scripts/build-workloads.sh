#!/usr/bin/env bash
# Build the bare-metal workloads into build/workloads/*.elf.
#   scripts/build-workloads.sh
# Needs a RISC-V gcc on PATH. The workloads are RV64I (the interrupt one adds Zicsr)
# and avoid multiply/divide, so they link with -nostdlib and no libgcc.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/build/workloads"
mkdir -p "$OUT"

CC=""
for p in riscv64-unknown-elf- riscv64-elf- riscv64-linux-gnu-; do
  command -v "${p}gcc" >/dev/null && { CC="${p}gcc"; break; }
done
[ -n "$CC" ] || { echo "error: no RISC-V gcc on PATH"; exit 2; }

build() {  # name, march, extra sources...
  local name="$1" march="$2"; shift 2
  "$CC" -O2 -march="$march" -mabi=lp64 -mcmodel=medany -ffreestanding -fno-builtin \
        -fno-tree-loop-distribute-patterns -nostdlib -nostartfiles -static \
        -Wl,--no-warn-rwx-segments \
        -T "$ROOT/workloads/link.ld" -o "$OUT/$name.elf" \
        "$ROOT/workloads/crt0.S" "$@" "$ROOT/workloads/$name.c"
  echo "built $OUT/$name.elf"
}

build coremark_like rv64i
build chacha20      rv64i
build irq_loop      rv64i_zicsr "$ROOT/workloads/irq_entry.S"
