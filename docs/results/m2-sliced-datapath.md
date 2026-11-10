<!-- Split from docs/results.md on 2026-11-10 without changes to the text; the index of all result sections is docs/results.md. -->

## M2: sliced ALU and sliced register file, zero faults

Everything below is simulation with no faults injected. Cycle numbers are model
estimates from tier-C assumptions (`config/params.toml`, `[cycles.*]`), not
measurements.

### Correctness (zero faults)

| Check | Result |
|---|---|
| Sliced ALU vs an independent reference, every op and W form, all 8-bit operand pairs, K = 1, 2, 4 | pass |
| Same at 16 bits: every `a` against 40 structured and random `b`, K = 2, 8 | pass |
| 64 bits, K = 8, 16, 32: every pair from a 272-value boundary pool (every slice and word boundary), every shift amount 0..63 (with junk in the upper amount bits), 300,000 seeded random pairs per op | pass |
| 64-bit results also compared directly with the golden `alu_reference()` | pass |
| Beat accounting: one slice-unit call per beat, for every op | pass |
| 119,504,353 ALU checks in total | 0 failures |
| Sliced regfile: random read/write round trip, parity detects a single flipped bit, a remapped slice lives in its spare | pass |
| Lockstep with the golden core, per step, 30 random programs of ~800 instructions (all ALU forms, loads, stores, forward branches) at each K | pass (149,504 checks) |
| riscv-tests rv64ui (54) and rv64mi (11), hardware model installed, K = 8, 16, 32 | 65/65 at each K |
| Same with a plain (fault-free) register file under the sliced ALU, K = 16: the hybrid baseline | 65/65 |

The 2 rv64mi tests the golden runner already excludes (`breakpoint`, `pmpaddr`) stay
excluded, for the same reasons.

**Test sensitivity.** I deliberately broke the ALU three ways (SRA fill always zero,
sign extension from the wrong bit, SUB without the +1) and each was caught by the unit
tests with millions of failures. A fourth break (SUBW without the +1) was caught by
the riscv-tests integration: `rv64ui-subw` failed while 53/54 passed.

### Cycle overhead versus a full 64-bit datapath (model estimate)

Overhead = cycles(sliced) / cycles(64-bit). `base` is the tier-C cost of everything
outside the integer ALU, in beats per instruction. Source: `python scripts/m2_cycles.py`
(also writes `results/m2_cycles.csv`).

| program | base | K = 8 | K = 16 | K = 32 |
|---|---:|---:|---:|---:|
| alu_mix (compute loop, 2M instr) | 1 | 4.22x | 2.36x | 1.43x |
| alu_mix | 2 | 3.15x | 1.91x | 1.29x |
| alu_mix | 4 | 2.29x | 1.54x | 1.17x |
| rv64ui + rv64mi test programs | 1 | 4.00x | 2.27x | 1.41x |
| rv64ui + rv64mi | 2 | 2.95x | 1.83x | 1.26x |
| rv64ui + rv64mi | 4 | 2.15x | 1.49x | 1.16x |

Mean beats per ALU op: 7.4 to 7.5 at K = 8, 3.7 to 3.8 at K = 16, about 1.9 at K = 32 (W forms are half-length).
About 0.85 to 1.0 ALU operations are issued per instruction.

### What was not shown
- No faults: nothing here says anything about defects, parity, or radiation. That is M3 and M4.
- 64-bit exhaustiveness is not claimed (it is infeasible); see G3 in `gaps.md`.
- The cycle model counts one beat per slice operation independent of K. A real wider
  slice is a slower slice, which would narrow the advantage of larger K. It also assumes
  the register file streams at the ALU slice width, and that W-form sign extension is free.
  The test programs are short and not representative, and `alu_mix` is a synthetic loop,
  so neither is a CoreMark-class workload.
- Only the ALU is sliced. The PC adders, M/A/F/D and CSR operations stay plain (a scope
  decision), so the overhead excludes them.
- The register file allocates a word for x0 (which the CPU hardwires), so it has 256 slices
  rather than 248.
- The sliced register file's parity bit is stored outside the slice array and is treated
  as fault-free for now.

### Failures and rejected ideas along the way
- My first lockstep test passed in 0.1 s with a suspiciously low check count. The random
  programs were trapping early because `lui` sign-extended the scratch base address, so
  every load faulted and only a program prefix ran. Fixed, and the test now fails if the
  golden core traps or if fewer than 700 instructions executed.
- A second flaw in the same test: a fixed step count ran past the end of the program after
  taken branches. It now runs to the program's end address.
- In riscv-emu's own backend test (on `radslice-hooks`), my "does the backend steer the
  result" check failed on first run because two bit-flipped adds cancelled each other. The
  bug was in the test, not the emulator; it now uses one instruction.
- Rejected: counting W-form sign extension as an extra beat. There is no source to
  justify a cost for it, so it is a tier-C parameter set to 0 rather than a baked-in beat.
- Found, not fixed (not in scope, golden core untouched): `riscv_emu --dump` segfaults in
  the unmodified `main` build of riscv-emu on this Windows/MinGW setup.

### What would falsify these conclusions
- A single operand pair for which the sliced ALU differs from the golden ALU.
- Any riscv-tests failure that appears only with the hardware model installed.
- A cycle-accurate model of a real sliced datapath whose per-op cost is not proportional
  to the slice count.
