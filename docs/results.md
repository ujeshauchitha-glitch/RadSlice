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
their spare sweeps, and the combined chip yield): **largest |z| = 2.59, none outside 4
standard errors.** The simulated mean number of defective register slices is 74.20
against an expected 74.24 (= 256 x 0.29). Selected points:

| point | simulated | closed form |
|---|---:|---:|
| register file, 96 spares | 0.2609 | 0.2576 |
| register file, 112 spares | 0.7492 | 0.7481 |
| register file, 128 spares | 0.9711 | 0.9718 |
| register file, 144 spares | 0.9990 | 0.9990 |
| ALU K = 8, 6 spares | 0.9216 | 0.9204 |
| ALU K = 16, 8 spares | 0.9315 | 0.9309 |
| ALU K = 32, 4 spares | 0.4728 | 0.4758 |
| chip: 128 rf spares + ALU K = 8 with 10 spares | 0.9654 | 0.9674 |

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
Sampled defective chips (73 to 84 defective primary register slices and 1 to 4 defective
primary ALU units each, all remapped, using 160 register and 24 ALU spares; more defects
fell on spares) run the whole rv64ui + rv64mi
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
| 0.1 | 1 | 99.9% | 100% | 100% |
| 0.1 | 4 | 93.1% | 100% | 99.9% |
| 0.1 | 16 | 83.3% | 97.0% | 40.3% |
| 0.1 | 64 | 80.8% | 0% | 0% |
| 0.5 | 1 | 88.0% | 100% | 100% |
| 0.5 | 4 | 81.5% | 32.7% | 20.1% |
| 0.5 | 16 | 80.9% | 0% | 0% |

The apparent "yield" of a weak-defect population falls from 99.9% to 80.8% as the test
gets longer: the first number was never real. The functional check (3 register passes
and 300 random ALU ops) also misses some defective chips that the exact defect map shows
(32.7% latent, 20.1% failing), so passing a short functional test is weaker evidence than
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

- **A seeding flaw, found during M4 and fixed.** Per-row and per-run random seeds were
  built as `seed + golden_ratio * index`. The generator (splitmix64) advances its state by
  exactly that constant per draw, so neighbouring seeds gave the same stream shifted by one
  draw: the 65 yield points and the chip rows were not independent samples. Each point's own
  estimate was still unbiased, but the independence that the "no point outside 4 standard
  errors" claim assumes was not there. Seeds are now hashed (`Rng::stream`), a regression
  test shows the old construction overlapped in all 200 neighbouring pairs and the new one
  in none, and every M3 number in this section was regenerated. The conclusions did not
  change (largest |z| went from 2.20 to 2.59, the weak-defect and spare-area results are the
  same to within noise); the quoted figures moved by up to about one percentage point.

### What would falsify these conclusions
- A simulated yield more than 4 standard errors from the closed form, or a repaired
  chip that fails a test its defect-free twin passes.
- Real defect clustering strong enough that a shared pool saturates locally.
- A primary-source p far from 0.71, or ALU logic whose defect rate per bit differs
  from the register file's; the K comparison in particular depends on the second.
- A hardware cost for the spare-routing crossbar that grows fast enough with finer
  slices to cancel the yield advantage.

## M4: transient faults, protection schemes and fault-injection campaigns

Simulation only. The upset rates are tier-C parameters, swept, and are not measurements of
MoS2 or of any radiation environment. Reproduce with `bash scripts/build-workloads.sh` and
`python scripts/m4_campaign.py` (180 cells x 2000 runs, fixed seeds; writes
`results/m4_cells.csv`, `m4_summary.md` with every table, and `m4_combined.svg`).

### What was run
- **Workloads** (bare-metal, M-mode, RV64I): a CoreMark-style mix (43,376 instructions), a
  ChaCha20 kernel checked against an independently computed RFC 8439 block (47,993), and an
  interrupt-heavy timer loop taking 60 machine-timer interrupts (24,169).
- **Upsets:** single-bit flips. Register file: one Poisson process over every exposed stored
  bit (data of x1..x31 in every copy, plus the parity bit where the scheme has one), swept over
  1e-9, 3e-9 and 1e-8 per bit per model cycle. ALU: each slice-unit output bit (and the carry)
  per beat, swept over 5e-7, 2e-6 and 8e-6. At the middle rates the unprotected core sees
  about 1 register-file upset and a few ALU upsets per run.
- **Schemes:** register file none / parity (detect only, then fail-stop) / parity with two
  copies / TMR with a bitwise vote, each with and without a scrubber (one word per 32
  instructions); ALU none / DWC (run twice, a third time and vote on mismatch) / TMR (always
  three times); and a combination, `full` = register-file TMR + ALU DWC + scrub.
- **Outcomes**, each run in exactly one class: correct, detected-and-recovered, detected but
  not recovered (fail-stop; an addition to the original list, since parity alone can only end
  this way), silent data corruption (SDC), hang/crash, boot failure. "Succeeds" below means
  correct or detected-and-recovered. Every run is compared with the golden core in lockstep
  and its output window is compared at the end.
- **2000 runs per cell**; the 95% intervals are Wilson intervals. With 0 events in 2000 runs the
  upper bound is 0.2%.

### Checks on the experiment itself
- A re-run reproduced all 150 cells it shared with the first run **exactly** (identical counts).
- The injector's total upset count agrees with rate x exposure in every cell tested (no
  z-score beyond 4; an earlier version failed this check, which exposed the seeding flaw
  described under M3).
- Boot failures with permanent defects match the M3 closed form at all 12 points tested
  (largest |z| = 1.55).

### Register-file upsets only, 3e-9 per bit per cycle
| workload | scheme | succeeds | SDC % (95% CI) | fail-stop % | hang/crash % |
|---|---|---:|---|---:|---:|
| coremark_like | none | 67.4 | 9.4 (8.2-10.8) | 0.0 | 23.2 |
| coremark_like | parity | 56.1 | 0.0 (0.0-0.2) | 43.7 | 0.2 |
| coremark_like | parity+scrub | 36.0 | 0.0 (0.0-0.2) | 64.0 | 0.0 |
| coremark_like | dup | 99.5 | 0.0 (0.0-0.2) | 0.4 | 0.0 |
| coremark_like | dup+scrub | 99.8 | 0.0 (0.0-0.2) | 0.1 | 0.0 |
| coremark_like | tmr-rf | 100.0 | 0.0 (0.0-0.2) | 0.0 | 0.0 |
| coremark_like | tmr-rf+scrub | 100.0 | 0.0 (0.0-0.2) | 0.0 | 0.0 |
| chacha20 | none | 63.8 | 18.9 (17.2-20.6) | 0.0 | 17.3 |
| chacha20 | parity | 47.4 | 0.0 (0.0-0.2) | 52.5 | 0.1 |
| chacha20 | parity+scrub | 38.1 | 0.0 (0.0-0.2) | 61.9 | 0.1 |
| chacha20 | dup | 99.7 | 0.0 (0.0-0.2) | 0.3 | 0.0 |
| chacha20 | dup+scrub | 100.0 | 0.0 (0.0-0.2) | 0.1 | 0.0 |
| chacha20 | tmr-rf | 99.9 | 0.0 (0.0-0.2) | 0.0 | 0.1 |
| chacha20 | tmr-rf+scrub | 100.0 | 0.0 (0.0-0.2) | 0.0 | 0.0 |
| irq_loop | none | 91.6 | 2.5 (1.9-3.3) | 0.0 | 5.9 |
| irq_loop | parity | 76.5 | 0.0 (0.0-0.2) | 23.5 | 0.0 |
| irq_loop | parity+scrub | 61.7 | 0.0 (0.0-0.2) | 38.3 | 0.0 |
| irq_loop | dup | 100.0 | 0.0 (0.0-0.2) | 0.0 | 0.0 |
| irq_loop | dup+scrub | 99.8 | 0.0 (0.0-0.2) | 0.1 | 0.0 |
| irq_loop | tmr-rf | 100.0 | 0.0 (0.0-0.2) | 0.0 | 0.0 |
| irq_loop | tmr-rf+scrub | 100.0 | 0.0 (0.0-0.2) | 0.0 | 0.0 |

### ALU upsets only, 2e-6 per output bit per beat
This is also the hybrid case (a fault-free register file under MoS2 logic).

| workload | scheme | succeeds | SDC % (95% CI) | fail-stop % | hang/crash % |
|---|---|---:|---|---:|---:|
| coremark_like | none | 11.1 | 32.6 (30.6-34.7) | 0.0 | 56.3 |
| coremark_like | alu-dwc | 100.0 | 0.0 (0.0-0.2) | 0.0 | 0.0 |
| coremark_like | alu-tmr | 100.0 | 0.0 (0.0-0.2) | 0.0 | 0.0 |
| chacha20 | none | 3.4 | 51.0 (48.9-53.2) | 0.0 | 45.6 |
| chacha20 | alu-dwc | 100.0 | 0.0 (0.0-0.2) | 0.0 | 0.0 |
| chacha20 | alu-tmr | 100.0 | 0.0 (0.0-0.2) | 0.0 | 0.0 |
| irq_loop | none | 20.0 | 47.8 (45.6-50.0) | 0.0 | 32.2 |
| irq_loop | alu-dwc | 100.0 | 0.0 (0.0-0.2) | 0.0 | 0.0 |
| irq_loop | alu-tmr | 100.0 | 0.0 (0.0-0.2) | 0.0 | 0.0 |

### Both at once (3e-9 and 2e-6)
| workload | scheme | succeeds | SDC % (95% CI) | fail-stop % | hang/crash % |
|---|---|---:|---|---:|---:|
| coremark_like | none | 6.4 | 24.6 (22.8-26.5) | 0.0 | 69.0 |
| coremark_like | parity | 5.3 | 16.4 (14.8-18.1) | 36.5 | 41.7 |
| coremark_like | parity+scrub | 3.7 | 12.4 (11.0-13.9) | 54.4 | 29.5 |
| coremark_like | dup | 10.4 | 32.2 (30.2-34.3) | 0.4 | 56.9 |
| coremark_like | dup+scrub | 11.4 | 32.4 (30.3-34.4) | 0.2 | 56.0 |
| coremark_like | tmr-rf | 10.4 | 32.1 (30.1-34.2) | 0.0 | 57.4 |
| coremark_like | tmr-rf+scrub | 11.0 | 31.9 (29.9-34.0) | 0.0 | 57.1 |
| coremark_like | alu-dwc | 49.1 | 13.3 (11.9-14.9) | 0.0 | 37.6 |
| coremark_like | alu-tmr | 36.1 | 14.4 (12.9-16.0) | 0.0 | 49.5 |
| coremark_like | full | 100.0 | 0.0 (0.0-0.2) | 0.0 | 0.0 |
| chacha20 | none | 2.2 | 41.9 (39.8-44.1) | 0.0 | 55.9 |
| chacha20 | parity | 1.4 | 24.3 (22.5-26.2) | 40.2 | 34.1 |
| chacha20 | parity+scrub | 1.9 | 21.1 (19.4-23.0) | 47.6 | 29.3 |
| chacha20 | dup | 3.9 | 53.0 (50.8-55.2) | 0.3 | 42.9 |
| chacha20 | dup+scrub | 3.2 | 52.3 (50.2-54.5) | 0.1 | 44.4 |
| chacha20 | tmr-rf | 4.0 | 51.5 (49.4-53.7) | 0.0 | 44.4 |
| chacha20 | tmr-rf+scrub | 3.4 | 50.0 (47.9-52.2) | 0.0 | 46.6 |
| chacha20 | alu-dwc | 47.9 | 23.4 (21.6-25.3) | 0.0 | 28.7 |
| chacha20 | alu-tmr | 34.6 | 28.4 (26.5-30.4) | 0.0 | 37.0 |
| chacha20 | full | 100.0 | 0.0 (0.0-0.2) | 0.0 | 0.0 |
| irq_loop | none | 18.7 | 44.5 (42.4-46.7) | 0.0 | 36.8 |
| irq_loop | parity | 17.2 | 35.8 (33.7-37.9) | 20.7 | 26.4 |
| irq_loop | parity+scrub | 11.5 | 28.1 (26.2-30.2) | 39.6 | 20.8 |
| irq_loop | dup | 20.1 | 48.4 (46.3-50.6) | 0.0 | 31.5 |
| irq_loop | dup+scrub | 20.4 | 47.2 (45.1-49.4) | 0.1 | 32.3 |
| irq_loop | tmr-rf | 20.0 | 46.6 (44.4-48.8) | 0.0 | 33.4 |
| irq_loop | tmr-rf+scrub | 21.1 | 47.5 (45.3-49.7) | 0.0 | 31.4 |
| irq_loop | alu-dwc | 85.9 | 4.2 (3.4-5.2) | 0.0 | 9.9 |
| irq_loop | alu-tmr | 79.7 | 5.7 (4.7-6.7) | 0.0 | 14.7 |
| irq_loop | full | 100.0 | 0.0 (0.0-0.2) | 0.0 | 0.0 |

### What each scheme costs (fault-free, model estimates)
Cycle overhead is from a fault-free run of each scheme against the unprotected sliced core.
Register-file voting and parity checks are assumed free; a scrub read costs 1 beat; ALU
re-execution costs real beats.

| scheme | RF storage | cycles, coremark_like | chacha20 | irq_loop |
|---|---:|---:|---:|---:|
| none | 1.00x | 1.000x | 1.000x | 1.000x |
| parity | 1.02x | 1.000x | 1.000x | 1.000x |
| parity+scrub | 1.02x | 1.007x | 1.008x | 1.008x |
| dup | 2.03x | 1.000x | 1.000x | 1.000x |
| dup+scrub | 2.03x | 1.007x | 1.008x | 1.008x |
| tmr-rf | 3.00x | 1.000x | 1.000x | 1.000x |
| tmr-rf+scrub | 3.00x | 1.007x | 1.008x | 1.008x |
| alu-dwc | 1.00x | 1.762x | 1.728x | 1.741x |
| alu-tmr | 1.00x | 2.525x | 2.456x | 2.483x |
| full | 3.00x | 1.770x | 1.736x | 1.749x |

### Manufacturing yield cost: boot failure with permanent defects (M3 model, 12 ALU spares)
Each register-file copy must be repaired separately.

| RF spares per copy | none | dup (2 copies) | tmr (3 copies) |
|---:|---:|---:|---:|
| 96 | 76.0% | 93.2% | 98.4% |
| 112 | 25.9% | 45.0% | 57.9% |
| 128 | 3.5% | 6.9% | 8.8% |
| 160 | 0.9% | 1.0% | 0.8% |

### Findings
1. **The unprotected core fails often, mostly by crashing or hanging, not by quietly corrupting.**
   At 3e-9, 9.4% / 18.9% / 2.5%
   of runs end in SDC (coremark / chacha20 / irq_loop) and
   23.2% / 17.3% / 5.9% hang or crash.
2. **Parity alone converts silent failures into halts, not into working runs.** SDC falls to about
   zero, but 44% / 53% / 24% of runs
   fail-stop. It is a safety measure, not an availability one.
3. **A second copy with parity, or TMR, restores nearly every run.** At 3e-9, success is
   99.5% / 99.7% / 100.0% (dup)
   and 100.0% / 99.9% / 100.0% (TMR), with no SDC in 2000 runs per cell
   (95% upper bound 0.2%). The same holds at the lowest and highest register-file rates, so
   it is not an artefact of one rate, with one exception: at 1e-8 there are 2 SDC runs of 2000
   on coremark_like, for both dup and TMR (upper bound 0.4%). Those are the double upsets the
   schemes cannot correct, and they are the first sign of saturation as the rate rises.
4. **Scrubbing helps the schemes that can correct, and hurts the one that cannot.** At 1e-8 it
   cuts parity-dup's fail-stops from 8.2% to 1.4% (coremark) and 2.5% to 0.7% (chacha20),
   because single upsets no longer accumulate into uncorrectable ones. With parity only, scrubbing
   *raises* fail-stops (44% to 64% on coremark at 3e-9)
   by halting on upsets in registers that would never have been read: a new failure mode, as
   anticipated in `docs/gaps.md` G8.
5. **ALU re-execution removes ALU-induced failures in this model.** At 2e-6 the unprotected ALU
   gives 32.6% / 51.0% / 47.8% SDC and
   56.3% / 45.6% / 32.2% hang or crash; DWC and TMR succeed in
   100% or nearly 100% of runs. DWC costs about 1.7x cycles and TMR about 2.5x, for no
   measurable extra protection here, because the voter is assumed fault-free and re-executions
   are assumed independent. On these numbers TMR is not worth its extra 40% of cycles over DWC.
6. **Protecting only one side barely helps when both are being upset.** At the middle rates,
   register-file TMR alone lifts coremark success only from 6.4% to 10.4%, and ALU DWC alone
   to 49.1%. SDC even *rises* under register-file-only protection
   (24.6% to 32.1% on coremark) because runs that used to crash from a register
   upset now survive long enough to be corrupted by an ALU upset. SDC alone is therefore a
   misleading summary; the outcome mix is the result. Only `full` succeeds in every run
   (0 failures in 2000 for each workload, upper bound 0.2%), at 1.77x cycles and 3x register storage.
7. **Protection costs yield.** TMR needs three repaired register files, so at 128 spares per
   copy boot failure rises from 3.5% to 8.8%, and at 112 spares from 25.9% to 57.9%
   (matching the closed form), before counting that each copy also carries its own spares.
8. **For the research question (G6/G8):** the protection schemes reduce silent corruption and
   hangs by far more than the 95% intervals at every rate swept, which is what G8 predicted; the
   costs are storage (2x, 3x), ALU beats (1.7x, 2.5x), a fail-stop failure mode, and
   manufacturing yield. M4 cannot say whether MoS2 is more or less upset-prone than silicon.

### What was not shown
- **Real upset rates.** Everything depends on the swept tier-C rates. The 100% results hold only
  where the model's assumptions hold.
- **What is not injected.** The PC, memory, CSRs, the control FSM, and the parity/vote/compare
  logic are fault-free, so the residual failures a real core would have are exactly the ones
  not modelled. Upsets are independent single-bit flips: no multi-bit or correlated upsets,
  which is why parity-only SDC is near zero (an even number of flips in one word is the case
  it cannot see, and is rare here).
- **Time redundancy** assumes an upset affects one execution of an operation and not the next.
- **One ALU width (K = 16)** and one defect model; the K trade-off is in M2 and M3.
- **The interrupt workload** takes 60 interrupts and is short; coremark_like is not CoreMark.
- **Cycles** are model estimates: parity and vote logic are free, a scrub read is 1 beat.
- **Lockstep** compares the PC and the written register each instruction and is used to date the
  first divergence, but classification depends only on the final output, so the lockstep
  statistics (error latency) were collected and not analysed. One test mutant that removes the
  PC comparison is not caught by the tests; the effect would be a slightly later reported
  first divergence, not a different outcome.
- The defects campaign uses one workload (coremark_like).

### Failures and corrections along the way
- A parity detection could be reported as a crash: when a corrupted stack pointer made the same
  instruction fault, the harness classified the run as a crash before looking at the parity
  flag. Hardware flags the read first; the order is fixed and a test pins it.
- A crash was not counted as a lockstep divergence. Fixed.
- The injection check failed on my first cut (counts off by up to 5 sigma). The cause was
  correlated per-run seeds, described under M3; it affected M3 as well, and both were fixed.
- My first campaign table reported "cycles" as the mean over all runs, which mixes runs that
  stopped early (below 1x) with runs that hung (above 1x) and showed parity at 0.54x. It was
  replaced by the fault-free overhead above. The defects table also lacked its spares column
  on first generation.
- A test for the SDC class did not exist until I saw SDC was common in the data; the invariant
  tests (SDC means the output differs; correct means identical output and no correction)
  were added afterwards.

### What would falsify these conclusions
- Multi-bit or clustered upsets that defeat parity and TMR at rates that matter.
- Upsets in the vote, compare and parity logic, the PC, memory or CSRs at comparable rates.
- Real rates so low that none of this matters, or so high that triple copies saturate.
- An ALU upset that persists across re-executions (a short but not instantaneous transient).
- A cost model in which register-file voting or ALU compare is not free.
