#!/usr/bin/env bash
# Assemble rtl/demo/demo.S (RV64I) and write the 32-bit-word memory image the DE2-115 wrapper loads.
#   bash scripts/build-demo.sh                 -> rtl/de2_115/demo.hex
#   DELAY=20 OUT=build/core-tmp/demo-fast.hex bash scripts/build-demo.sh    (a fast-counting image for simulation)
set -euo pipefail
cd "$(dirname "$0")/.."
CC=""
for p in riscv64-unknown-elf- riscv64-elf- riscv64-linux-gnu-; do
  command -v "${p}gcc" >/dev/null && { CC="${p}"; break; }
done
[ -n "$CC" ] || { echo "error: no RISC-V gcc on PATH"; exit 2; }
tmp=build/core-tmp; mkdir -p "$tmp"
"${CC}gcc" -march=rv64i -mabi=lp64 -nostdlib -nostartfiles -static -Wl,--no-warn-rwx-segments \
  -DDELAY="${DELAY:-20000}" -T rtl/demo/link.ld -o "$tmp/demo.elf" rtl/demo/demo.S
"${CC}objcopy" -O binary "$tmp/demo.elf" "$tmp/demo.bin"
python - "$tmp/demo.bin" "${OUT:-rtl/de2_115/demo.hex}" <<'PY'
import struct, sys
d = open(sys.argv[1], 'rb').read()
d += b'\0' * (-len(d) % 4)
words = struct.unpack('<%dI' % (len(d) // 4), d)
open(sys.argv[2], 'w').write('\n'.join('%08x' % w for w in words) + '\n')
print(len(words), 'words ->', sys.argv[2])
PY
"${CC}objdump" -d "$tmp/demo.elf" | sed -n '1,40p'
