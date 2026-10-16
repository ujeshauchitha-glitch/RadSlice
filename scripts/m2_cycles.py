"""Analytical cycle-overhead estimate for the sliced ALU (milestone M2).

NOT a measurement. Runs each program on the hardware model, collects instruction
and ALU-beat counts from radslice_run, and applies the model

    cycles(K)    = instret * base + beats(K)       one beat per slice-unit operation
    cycles(64)   = instret * base + alu_ops        a full 64-bit datapath: one beat per op

where `base` is the cost, in beats, of everything outside the integer ALU
(fetch, decode, memory, the plain PC adders, M/A/F/D). `base` is a tier-C model
parameter, so the result is reported for several values of it.

Usage (from the repo root, after building and after the riscv-tests ELFs exist):
    python scripts/m2_cycles.py
"""
import csv
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EMU = next((p for p in (ROOT / "build/radslice_run", ROOT / "build/radslice_run.exe") if p.exists()), None)
TESTS = ROOT / "extern/riscv-emu/build/riscv-tests"
LINK = ROOT / "extern/riscv-emu/tests/riscv-tests-env/link.ld"
KS = (8, 16, 32)
BASES = (1, 2, 4)
STAT = re.compile(r"RADSLICE_STATS (.*)")


def run(elf, k, max_steps):
    env = dict(os.environ, RADSLICE_K=str(k), RADSLICE_ALU="sliced", RADSLICE_RF="sliced")
    out = subprocess.run([str(EMU), "--max-steps", str(max_steps), str(elf)],
                         capture_output=True, text=True, env=env).stderr
    m = STAT.search(out)
    if not m:
        sys.exit(f"no stats from {elf}:\n{out}")
    kv = dict(f.split("=") for f in m.group(1).split())
    return int(kv["instret"]), int(kv["alu_ops"]), int(kv["beats"]), "PASS" in out


def build_workload():
    gcc = next((c for c in ("riscv64-unknown-elf-gcc", "riscv64-elf-gcc") if subprocess.run(
        ["where" if os.name == "nt" else "which", c], capture_output=True).returncode == 0), None)
    if not gcc:
        sys.exit("no RISC-V gcc on PATH")
    out = ROOT / "build/alu_mix.elf"
    subprocess.run([gcc, "-march=rv64i_zicsr", "-mabi=lp64", "-nostdlib", "-nostartfiles",
                    "-T", str(LINK), "-o", str(out), str(ROOT / "workloads/alu_mix.S")], check=True)
    return out


def main():
    if EMU is None:
        sys.exit("build radslice_run first")
    suites = sorted(p for p in TESTS.glob("rv64u[i]-*.elf")) + sorted(TESTS.glob("rv64mi-*.elf"))
    if not suites:
        sys.exit("no riscv-tests ELFs; run scripts/run-hw-riscv-tests.sh first")
    programs = {"rv64ui+rv64mi (test programs)": (suites, 2_000_000),
                "alu_mix (compute loop, 2M instr)": ([build_workload()], 2_000_000)}

    rows = []
    for name, (elfs, steps) in programs.items():
        for k in KS:
            instret = ops = beats = 0
            for elf in elfs:
                i, o, b, ok = run(elf, k, steps)
                if "alu_mix" not in name and not ok:
                    sys.exit(f"{elf.name} did not pass at K={k}")
                instret += i; ops += o; beats += b
            for base in BASES:
                c64 = instret * base + ops
                ck = instret * base + beats
                rows.append((name, k, base, instret, ops, beats, c64, ck, ck / c64))

    out = ROOT / "results/m2_cycles.csv"
    out.parent.mkdir(exist_ok=True)
    with open(out, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["program", "K", "base_cycles", "instret", "alu_ops", "beats",
                    "cycles_64bit", "cycles_sliced", "overhead_x"])
        w.writerows(rows)

    print("MODEL ESTIMATES (tier-C cycle model), not measurements\n")
    print(f"{'program':36} {'K':>3} {'base':>4} {'ops/instr':>9} {'beats/op':>8} {'overhead':>9}")
    for name, k, base, instret, ops, beats, c64, ck, ov in rows:
        print(f"{name:36} {k:3d} {base:4d} {ops/instret:9.3f} {beats/ops:8.2f} {ov:8.3f}x")
    print(f"\nwritten: {out}")


if __name__ == "__main__":
    main()
