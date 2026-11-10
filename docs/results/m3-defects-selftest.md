<!-- Split from docs/results.md on 2026-11-10 without changes to the text; the index of all result sections is docs/results.md. -->

## M3: permanent defects, boot self-test and remap

Simulation only; the defect model is tier C throughout (`config/params.toml`,
`[defects.*]`, `[selftest.*]`, `[spares.*]`). Reproduce with `python scripts/m3_yield.py`
(fixed seeds; writes `results/m3_yield.csv`, `m3_yield.svg`, `m3_weak.csv`,
`m3_weak_chips.csv`). Nothing here is measured.

### What was simulated
Each physical slice is defective independently with probability 1 - p (p = 0.71 for an
8-bit register slice; unverified when these runs were made and later calibrated to the paper at the same value, so nothing here changes). A defective slice has one stuck output bit or a stuck
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
