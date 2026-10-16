# Results

Results come from scripts in this repository with fixed seeds. Every number is
a simulation under tier-C assumptions, not a measurement. Rejected ideas and
failures are recorded here, not deleted.

## M0: closed-form yield model

Source: `python -m radslice.yield_model` (run from `python/`), `p = 0.71`
(tier C, unverified secondary figure), 32 registers x 8 byte slices = 256 slices.

- 64-bit register, 0 spares: 0.71^8 = 0.0646.
- Whole-file yield, with the pool treated as an ideal crossbar:

| spares | area  | per-register spares | shared pool |
|-------:|------:|--------------------:|------------:|
| 0      | 1.00x | 0.0000              | 0.0000      |
| 16     | 1.06x | n/a (uneven split)  | 0.0000      |
| 32     | 1.12x | 0.0000              | 0.0000      |
| 64     | 1.25x | 0.0000              | 0.0002      |
| 96     | 1.38x | 0.0000              | 0.2576      |
| 128    | 1.50x | 0.0001              | 0.9718      |

- Monte Carlo check (20000 trials, seed 12345) of the 128-spare pool:
  0.9707 +/- 0.0012 against the closed form 0.9718, within 1 sigma.

**Not shown.** Remap-table and mux area/defects are ignored, so the shared-pool
column is an upper bound. The pool yield has a sharp cliff: 26% at 96 spares
versus 97% at 128, so a modest error in `p` moves the answer a lot. The
"1.5x" area counts register slices only. x0 needs no storage, so 256 slices
overstates the file by one register.

**Would falsify.** A clustered-defect model, or a primary-source `p` far from
0.71 (see G1, G2).

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

## M3: permanent defects, boot self-test and remap

Simulation only; the defect model is tier C throughout (`config/params.toml`,
`[defects.*]`, `[selftest.*]`, `[spares.*]`). Reproduce with `python scripts/m3_yield.py`
(fixed seeds; writes `results/m3_yield.csv`, `m3_yield.svg`, `m3_weak.csv`,
`m3_weak_chips.csv`). Nothing here is measured.

### What was simulated
Each physical slice is defective independently with probability 1 - p (p = 0.71 for an
8-bit register slice, unverified). A defective slice has one stuck output bit or a stuck
carry. The chip's real boot self-test then runs on the real register-file and ALU models
(every slice, primaries and spares), and defective primaries are remapped onto good
spares. A chip is repaired if enough good slices exist. The register file and the ALU
each have their own spares, and chip yield is the product.

### Simulated yield agrees with the closed form
20,000 chips per point, 65 points (11 register-file spare counts, 3 ALU widths with
their spare sweeps, and the combined chip yield): **largest |z| = 2.20, none outside 4
standard errors.** The simulated mean number of defective register slices is 74.22
against an expected 74.24 (= 256 x 0.29). Selected points:

| point | simulated | closed form |
|---|---:|---:|
| register file, 96 spares | 0.2597 | 0.2576 |
| register file, 112 spares | 0.7500 | 0.7481 |
| register file, 128 spares | 0.9726 | 0.9718 |
| register file, 144 spares | 0.9989 | 0.9990 |
| ALU K = 8, 6 spares | 0.9186 | 0.9204 |
| ALU K = 16, 8 spares | 0.9322 | 0.9309 |
| ALU K = 32, 4 spares | 0.4684 | 0.4758 |
| chip: 128 rf spares + ALU K = 8 with 10 spares | 0.9671 | 0.9674 |

### Granularity: finer slices need far less spare area
Closed form (confirmed by the simulation above), ALU spare area needed for a target yield:

| target | K = 8 | K = 16 | K = 32 |
|---|---:|---:|---:|
| 90% | 6 spares, 1.75x | 8 spares, 3.0x | 12 spares, 7.0x |
| 99% | 9 spares, 2.1x | 12 spares, 4.0x | 22 spares, 12x |

**The tradeoff.** Finer slices are much cheaper to make yield, and much slower. The M2
cycle model (alu_mix, base = 1) gives overheads of 4.22x, 2.36x and 1.43x for K = 8, 16,
32. So reaching 99% ALU yield costs about 2.1x ALU area at 4.2x cycles with K = 8, against
12x ALU area at 1.4x cycles with K = 32. Neither point is free, and which wins depends on
the cost of area against the cost of cycles, which is not something this project knows.
The multiplexing that lets a spare serve any slice grows with the number of slices, so
K = 8 would pay more for it than the ideal-crossbar assumption admits.

### Mechanism checks
- Every modelled single fault is detected: all 16 stuck-at faults of an 8-bit register
  slice, and for K = 8, 16, 32 every stuck data bit and both stuck-carry faults of an ALU unit.
- A planted defect really corrupts results before the repair and not after; a defective
  spare is skipped; too few spares rejects the chip.
- Three deliberate breaks of the repair code (all bad slices sharing one spare, carry
  not compared, defective spares treated as good) were each caught by the tests.
- With hard defects, 0 of the chips that passed the self-test later computed wrongly
  (4,000 chips, K = 16, 128 + 6 spares).

### End to end on riscv-tests
Sampled defective chips (about 80 defective register slices and 2 to 5 defective ALU
units each, repaired with 160 register and 24 ALU spares) run the whole rv64ui + rv64mi
suites **65/65 at K = 8, 16 and 32, for seeds 1, 2 and 3 (9 chips).**
Control: the same chips with the self-test turned off pass **0/65** (seeds 1 and 2,
K = 16). A defective unrepaired chip does not return wrong answers politely: on
`rv64ui-add` it ran until the step budget ran out, with 4 to 7 register-file parity
errors flagged. With no spares the self-test correctly reports "BOOT FAILURE".

### Weak (intermittent) defects escape a short self-test
A weak defect shows with probability q on each access. One defective register slice is
missed after R rounds with probability (1 - q)^(2R) (two of the four patterns expose a
given stuck bit). Simulation matches on 20 points, all within 4 standard errors
(q = 0.05 to 0.5, R = 1 to 16). At chip level (128 + 6 spares, K = 16, 5,000 chips per row):

| q | rounds | chips passing the test | of those, a defective slice still in use | of those, failing a functional check |
|---:|---:|---:|---:|---:|
| 0.1 | 1 | 99.8% | 100% | 100% |
| 0.1 | 4 | 92.8% | 100% | 99.8% |
| 0.1 | 16 | 82.9% | 97.5% | 41.0% |
| 0.1 | 64 | 80.4% | 0% | 0% |
| 0.5 | 1 | 88.0% | 100% | 100% |
| 0.5 | 4 | 82.1% | 33.3% | 21.2% |
| 0.5 | 16 | 81.1% | 0% | 0% |

The apparent "yield" of a weak-defect population falls from 99.8% to 80.4% as the test
gets longer: the first number was never real. The functional check (3 register passes
and 300 random ALU ops) also misses some defective chips that the exact defect map shows
(33.3% latent, 21.2% failing), so passing a short functional test is weaker evidence than
a clean self-test.

### What was not shown
- Defects are independent. Clustered defects (G1) change every yield number here and
  are not modelled.
- The fault model is one stuck bit. Delay, bridging and multi-bit defects, defects in
  the remap table or the self-test logic itself, and defects that appear after boot
  are not covered. Transient faults are M4.
- The remap table, its multiplexers and the test logic are fault-free and free, so all
  yields are upper bounds, and the area comparison between K values leaves out the cost
  of the crossbar.
- The self-test runs only at boot. Its duration, and any cycle cost of remapped access,
  are not modelled.
- The ALU defect probability p^(K/8) applies a register-slice figure to logic.
- x0's word is counted in the 256 register slices (one word more than needed), and the
  per-word parity bit is not itself subject to defects.

### Failures and corrections along the way
- A planned check that "at least 2000 chips pass and are verified" failed on my first
  run: with only 2 ALU spares at K = 16, about a third of chips pass. The expectation
  was wrong, not the model; the test now uses 6 spares.
- A first runner command printed nothing because the shell lacked the MSYS2 runtime on
  its PATH (exit code 127), and a few edits did not apply on the first try because of
  string escaping in my own scripts. None produced a result; each was found by checking
  the output rather than assuming it.
- The functional check turned out to be a weaker detector than the defect map, which is
  why the latent-defect count is reported separately.

### What would falsify these conclusions
- A simulated yield more than 4 standard errors from the closed form, or a repaired
  chip that fails a test its defect-free twin passes.
- Real defect clustering strong enough that a shared pool saturates locally.
- A primary-source p far from 0.71, or ALU logic whose defect rate per bit differs
  from the register file's; the K comparison in particular depends on the second.
- A hardware cost for the spare-routing crossbar that grows fast enough with finer
  slices to cancel the yield advantage.
