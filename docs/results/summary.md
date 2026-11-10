<!-- Split from docs/results.md on 2026-11-10 without changes to the text; the index of all result sections is docs/results.md. -->

## M5: What was shown, what was not, and what would change it

Read this first; the milestone sections below it hold the evidence.

**Status of the evidence.** Everything here is simulation. The register file and ALU are models
beneath an unmodified golden RV64IMAFDC emulator. Of the 54 parameters in `config/params.toml`, 41
are tier C (hypothetical). (Count as written at M5; on 2026-11-10 the file has 57 parameters, 8 tier A, 5 tier B,
44 tier C, after the G17 additions; `docs/assumptions.md` is the generated, tested count.) The thirteen sourced ones (8 tier A, 5 tier B) are all from one paper: eight
about yield (one of them, a bound on clustering, derived from its wafer map) and five transistor counts
used by the cost model (G10): `p = 0.71` for an 8-bit register slice was an unverified secondary figure when M0 to M5
were done, and was afterwards checked against the paper's main text and Supplementary Information
(section "Checking p against the primary source" below). It was confirmed, and so was the 64-bit
figure (about 7%). Every upset rate and every cost-model choice is still tier C. See
`docs/assumptions.md` (generated from the parameters). So each conclusion below means "in this model,
at these parameters", and the last column says what would overturn it.
No novelty is claimed: spare slices, parity, scrubbing and TMR all exist in silicon. What this
project contributes is a combined, simulated study of them on a slice-serial RV64 core under a
2D-style defect model, with the negative and awkward results kept.

### Conclusions

| # | Conclusion | Evidence | Rests on | Would be overturned by |
|---|---|---|---|---|
| C1 | Yield collapses with width: a 64-bit register (8 byte slices) with no spares yields 0.71^8 = 0.0646. The paper measured about 7% for a 64-bit (1,152-transistor) register; its 8-bit and 64-bit yields imply the same per-transistor yield, as independent failures predict. | M0 closed form; M3 simulation; the paper's main text and wafer map | independent slice defects at p = 0.71 (calibrated to the paper; a measured shift-register block, not a random-access register-file slice) | clustering above register scale (none seen on the one wafer map; one unexplained ordering pattern in a small inverter dataset), or a register-file slice yielding unlike the measured shift register |
| C2 | A shared spare pool beats per-register spares at equal spare count: whole-file yield 0.97 (128 spares, 1.5x slices) against about 0.0001 for 4 private spares per register. It still wins under every clustering tested, but 0.97 is a best case (see C12). | M0, M3, G9 | an ideal crossbar (any spare serves any slice); fault-free remap logic; no die-to-die variation of the defect rate (one wafer cannot test it) | spare reach limited to regions (8 regions: 0.16 at 128 spares even with independent defects); die-to-die variation (99% then needs about 2x slices); a crossbar cost that erases the gain (costed in G10: it does not erase it, but the best design is not the ideal pool; see C13) |
| C3 | A bit-sliced RV64I ALU (K = 8, 16, 32) and byte-sliced register file are functionally exact with zero faults. | M2: 119.5M ALU checks against an independent reference (exhaustive at 8 bits, dense at 16 and 64), lockstep on random programs, rv64ui + rv64mi 65/65 at every K; deliberate bugs caught | the reference and the scope (data ops; PC adders, M/A/F/D and CSRs stay plain) | a single operand pair that differs; 64-bit exhaustiveness is not claimed |
| C4 | Slice-serial execution costs cycles in proportion to slice count: about 4.2x, 2.4x, 1.4x for K = 8, 16, 32 on a compute loop (base cost 1), less as the non-ALU cost grows. | M2 analytical model | four tier-C cost choices (non-ALU cost, one beat per slice op regardless of K, free W-form sign extension, wide enough register port) | a cycle-accurate model showing wider slices are slower per beat, which narrows the gap |
| C5 | A boot self-test plus remap recovers yield to the closed form: 65 simulated points agree within Monte Carlo error (largest abs z 2.59); repaired chips pass the riscv-tests, unrepaired ones fail them. | M3 | the one-stuck-bit defect model; fault-free test logic; independent defects for the closed-form agreement (with clustering, the exact clustered model and the simulator agree on 23 further cases, though the yield itself is lower) | defects the test cannot see (delay, bridging, multi-bit, defects in the remap/test logic) |
| C6 | Finer ALU slices need far less spare area for the same yield (99% needs about 2.1x, 4x, 12x ALU area at K = 8, 16, 32), but cost more cycles (C4). Neither wins outright. | M3 closed form + simulation; M2; G9 (ordering unchanged by clustering, areas grow) | p^(K/8) scaling of an ALU slice from a register-slice figure (area-proportional scaling is supported for register-like circuits by the paper's 8-bit and 64-bit yields; ALU logic itself is not measured); ideal crossbar | logic with a different defect density per bit; a crossbar whose cost grows fast with slice count (G10: the ALU areas quoted are for the replicated organisation; see C13) |
| C7 | Weak (intermittent) defects escape a short self-test: with q = 0.1, one round passes 99.9% of chips and every one still has a defective slice in use; yield falls to 80.8% only after 64 rounds. | M3 | single-bit weak-defect model | real defects that are either always visible or never |
| C8 | Parity with a second copy, or TMR, plus ALU re-execution, remove silent corruption and hangs in this model at every rate swept, at a cost in storage (2x, 3x), ALU cycles (1.7x DWC, 2.5x TMR), a fail-stop failure mode, and yield. | M4: 180 cells x 2000 runs, reproduced exactly on re-run | independent single-bit upsets; fault-free vote/compare/parity logic; PC, memory, CSRs and control FSM not injected | multi-bit or clustered upsets; upsets in the protection logic or the unmodelled state; a persistent ALU upset |
| C9 | Scrubbing helps schemes that can correct and hurts parity-only (it halts the machine on upsets in registers never read again). | M4 | same as C8 | a scrub policy that only acts on registers about to be read |
| C10 | Protecting only one side barely helps when both are upset, and can raise silent corruption by letting runs survive to be corrupted by the other side. Only register-file TMR + ALU DWC + scrub succeeded in every run. | M4 | same as C8 | rates where one side dominates by orders of magnitude |
| C11 | Protection lowers manufacturing yield: TMR needs three repaired register files (boot failure 3.5% to 8.8% at 128 spares per copy; 25.9% to 57.9% at 112). If the copies share a die-level defect rate the cost is larger, not smaller (6.4 points rather than 0.2 at 144 spares per copy, alpha = 9.4). | M4 vs M3 closed form; G9 | defects independent per copy (otherwise see G9) | spares shared between copies |
| C12 | The shared-pool spare requirement is robust to clustering at the scales one wafer map can test (within a matrix, between neighbours) but not to die- or wafer-level variation of the defect rate, which it cannot test: at the strength the map allows between matrices (pairwise correlation about 0.03), a rate shared across the register file raises the spares for 99% yield from 134 to 248 (1.52x to 1.97x slices); the same strength in blocks of 32 slices raises it only to 147. | G9: exact model; the C++ simulator agrees on 23 cases (largest abs z 2.35) | gamma-mixed block model with the marginal yield held at p; ideal crossbar | measured die-to-die or wafer-to-wafer yield variation, which would fix the scale and strength |
| C13 | Costing the spare-routing crossbar changes the best register-file design and the K trade-off. Tagged routing costs about 10.8x the storage of the spares it serves; the cheapest silicon per working register file is 8 regions with 192 spares (4.21x the bare file per good file, against 7.11 for the 128-spare ideal pool and 1.54 if the crossbar were free). At chip level the register file is about 90% of the silicon, so ALU width moves silicon by at most 13% and speed decides: the widest sliced K (32) wins silicon x cycles, with 32 and 64 within 5% for a replicated ALU. (Rerun 2026-11-03 with the cycle cost measured on the Verilog core, base 21 instead of 2: the best K becomes 16, with 32 within 2%; see 'G18 follow-up: the K trade-off at the measured base'.) A single reused ALU unit needs far less silicon than the replicated organisation that M2 and M3 modelled, at the same cycle count. The routing's own transistors are treated as fault-free; if any failure there were fatal, no spare design would yield (5.5e-83 for the cheapest). | G10: exact models, transistor-count cost model with five paper-anchored or derived constants | transistor count as area; tagged-array steering as the design; fault-free routing | a cheaper or wire-dominated routing layout; routing with its own defect tolerance (would make the numbers attainable); an ALU 16x an adder (single unit then prefers K = 16) |
| C14 | The routing is made of transistors too, and counting that raises the cost of the cheapest register file by 1.4x (isolated-device per-transistor yield 0.9992) to 2.5x (0.99762, implied by the register yields) with the same 8 regions and more spares (192 to 280 to 568); a shared routing block of about 1,000 transistors yields 6% alone and needs replicating (a further 7% to 21%); a single reused ALU unit survives routing defects and a replicated ALU does not. | G14: exact model, reproduces G10 at q = 1 | independent defects; private routing per spare as designed (tagged array); a tier-C shared block; ideal selection among copies | a measured routing yield, a routing design that tolerates its own defects at lower cost, clustered routing defects |
| C15 | At the rates tried, flips in the PC are fatal 94% to 98% of the time (mostly crashes), flips in the fetched instruction 75% to 87% (about half silent), CSR flips about 4%, register-file bit flips 33%; SECDED memory removes single flips and turns bursts of 2 into detected stops but not bursts of 3 (worst case); register-file TMR corrects bursts of 1, 2 and 4 in one copy, parity does not detect 2 and parity-dup only detects them; online retest with a stall hurts more than it helps at high in-field defect rates; with every protection on, all runs complete correctly. | G15: M6 campaign, 400 runs per cell, 3 workloads | tier-C rates; fault-free voters, ECC, parity and retest logic; copies far enough apart that one event hits one copy; the workloads' small footprints | measured upset and defect rates; faults in the protection logic; larger memory footprints |
| C16 | The M4 ranking of protection schemes holds over five seeds and K = 8, 16, 32 (14 of 15 pairs identical in every seed; the other pair is not distinguishable at these rates); ALU duplication succeeds more at larger K (41%, 60%, 74%) because there are fewer beats to upset; pooled intervals understate the seed spread when workloads share streams. | G16: M6 campaign | same as C8 | a ranking flip at other rates |
| C17 | The paper's logic is n-type only with ratioed gates, so the transistor counts behind every cost already assume that style; a complementary style (mu = 4/3 to 2) raises the cost per good register file 1.3x to 1.9x and per good chip more, and leaves the register-file design (8 regions) and K* (32) unchanged. The process and space-environment literature (15 rows, mostly abstracts) says threshold drift, contact and gate-stack effects would produce logic-level and timing faults that the stuck-bit model and the boot self-test do not represent, and finds no single-event data for 2D circuits. | G11 (paper text and computation), G12 and G13 (literature survey) | the paper's text; a transistor-count multiplier; abstracts and excerpts rather than full reading | per-transistor yield of p-type devices; device-level data on thresholds under dose; a source on single-event effects in 2D circuits |

### The research question, answered as far as it can be
The gap (G6) asked whether a 2D-specific fault model, combined with yield sparing and transient-fault
handling on a slice-serial RV64 core, changes the outcome mix. **What was answered:** the structure
of the outcome mix (it depends strongly on protection scheme, workload and rate; C8 to C11), and
the costs of each scheme. **What was not, and cannot be, answered here:** whether MoS2 differs from
silicon. That needs primary-source defect and upset data, and this project has none; the only
2D-specific input is the tier-C yield figure. The hybrid baseline (fault-free silicon register file
under MoS2 logic) is the ALU-only campaign in M4; it shows ALU upsets alone cause most of the damage
at the swept rates, but whether those rates are realistic is exactly what is unknown.

### Gaps: where each stands
| Gap | Status |
|---|---|
| G1 yield collapses with width | supported (C1): the paper's main text reports 71% for 8-bit and about 7% for 64-bit registers, which agree with each other under independent failures; the one wafer map shows no clustering; how "working" was decided and any chip-level yield are not reported |
| G2 shared pool vs private spares | supported in-model (C2); costed in G10: the ideal pool is not the cheapest design (C13) |
| G3 sliced datapath is correct | supported (C3) |
| G4 cost of slice-serial execution | estimated, model-dependent (C4) |
| G5 self-test and remap | supported for hard defects (C5); weak defects escape (C7) |
| G5a granularity and spare area | supported on yield (C6); costed in G10: at chip level speed, not silicon, decides K (C13) |
| G6 combined yield + radiation on a slice-serial core | structure answered; the 2D-specific part is not answerable without data |
| G7 fair baseline | the hybrid case was run (the ALU-only campaign), but no head-to-head cost comparison (area, register-file yield) was made, so G7's falsifier (the hybrid wins on every metric) is neither confirmed nor ruled out |
| G8 protection reduces corruption, at what cost | supported in-model (C8 to C11), with the failure mode and yield cost found |
| G9 yield conclusions under clustered defects | four of five claims confirmed, one (TMR's penalty shrinking) not supported; the result depends on a scale (die-level variation) that one wafer cannot test (C12) |
| G11 logic style (n-only or complementary) | the paper's library is n-only (confirmed from the text); a complementary style costs mu = 4/3 to 2 times the transistors and yields p^mu per block, which raises the cost per good register file from 4.2x to 7.8x (own bare file); register-file design and K ordering unchanged (C17) |
| G12 process mechanisms | literature table (15 rows, mostly abstracts); stuck bit is probably not the only fault type: threshold and contact mechanisms predict logic-level and timing faults the model lacks (C17) |
| G13 barriers to use in space | table in the G12/G13 section: what RadSlice can represent (in-field defects, upsets) and cannot (threshold drift, dielectric charging, atomic oxygen, p-type); no single-event data for 2D circuits found (C17) |
| G14 the routing's own defects | spare routing raises the cheapest register file's cost 1.36x to 2.52x, the region count stays 8; a shared block needs replication; single-unit ALU survives, replicated does not (C14) |
| G15 more fault classes | claim 1 not supported as stated; claims 2 and 3 confirmed; claim 4 (retest needed) not supported at the rates tried (C15) |
| G16 seeds and K | rankings stable but one pair, intervals too narrow because workloads share streams (C16) |
| G10 crossbar cost and the K trade-off | claims 1 and 2 confirmed, 3 partly, 4 mostly robust; the routing's own defects are not modelled and could remove the benefit (C13) |

### Rejected ideas, failures and corrections (collected)
- Counting W-form sign extension as an extra beat: rejected for lack of a source; kept as a tier-C
  parameter set to 0 (M2).
- A test of my own that passed vacuously: lockstep random programs that trapped early and only
  ran a prefix; fixed and made to fail on early traps (M2).
- A wrong expectation in my tests (how many 2-spare chips pass), corrected (M3).
- **A seeding flaw that made per-run random streams shifted copies of each other.** Found because
  an injector sanity check was off by up to 5 sigma; it had also made the M3 yield points
  non-independent. Fixed, regression-tested, and M3 regenerated; conclusions unchanged (M3, M4).
- A parity detection was classified as a crash because of the order of two checks; a crash was not
  counted as a lockstep divergence; both fixed (M4).
- A misleading cycles column (mean over runs that stopped at different times) replaced by
  fault-free overhead (M4).
- Found, not fixed, out of scope: `riscv_emu --dump` segfaults in the unmodified golden build on
  this Windows/MinGW setup (M1).
- Known test gap: a mutant that drops the PC comparison from the lockstep check is not caught; it
  only changes when the first divergence is reported (M4).
- A hypothesis of mine that was wrong: that copies sharing a die would shrink TMR's yield penalty; it
  grows (G9). And a first reading of the wafer-map bound as covering die-level clustering, which it
  cannot (G9).

### Limits of the whole study (v0 scope, restated)
RV64I + Zicsr datapath only, machine-mode workloads only; the PC adders, M/A/F/D and CSR operations are not
sliced; a single hart; permanent defects independent (or gamma-clustered, G9) stuck bits; 256 register
slices including the word of x0. Since M6 the harness can also flip the PC, the fetched instruction, five CSRs
and the memory a program touches, flip adjacent bits in bursts, and create defects after boot (G15), and
M6 repeats the main comparison at K = 8, 16, 32 over five seeds (G16). Still not modelled: faults in the
voters, ECC logic, parity logic and the retest logic itself; threshold drift, timing faults and gate-stack
charging (G12, G13); static power and energy; the timing of any of it beyond the analytical beat model;
clustering of transient upsets and of routing defects.

### Open questions, in the order I would pursue them
1. (Done.) The paper's main text, Supplementary Information and source data have been read; see the
   section on checking p. What the paper does not report (how a register was judged working, chip-level
   yield, anything on radiation) cannot be resolved from it.
2. (Done, see G9.) What remains is the one thing it showed matters: how much the defect rate varies
   from die to die and wafer to wafer. That needs per-wafer yields, and the paper reports one wafer.
3. (Done, see G10 and G14.) What remains there is a measured per-transistor yield for routing, which
   sets the 1.4x to 2.5x range, and a defect-tolerant design of the shared routing block.
4. (Done, see G15.) The remaining injection targets are the voters, ECC and parity logic and the retest logic.
5. A fault type for threshold drift and for timing failures (G12 rows 1, 4 and 8), which needs a device-level
   source for a rate; and a static-power and energy model (G11).
6. Replace the analytical cycle model with a cycle-accurate one for at least one K.
7. Device and beam data for a circuit, not a transistor, and per-wafer yields. Nothing in the repository can supply them.

### Reproducing everything
```
git clone --recurse-submodules https://github.com/ujeshauchitha-glitch/RadSlice.git
cmake -S . -B build && cmake --build build -j      # C++ tests: ctest --test-dir build -R radslice_
bash scripts/run-python-tests.sh                   # closed forms, parameters, assumptions register
bash scripts/build-workloads.sh                    # needs a RISC-V gcc
bash scripts/run-hw-riscv-tests.sh 16 rv64ui rv64mi   # also K = 8 and 32
python scripts/m2_cycles.py                        # model-estimate cycle overhead (M2)
python scripts/m3_yield.py                         # about 25 s: yield vs spares, weak defects
python scripts/m4_campaign.py                      # roughly 10 to 15 min on 14 cores: 180 cells x 2000 runs
python scripts/cluster_study.py                    # about 35 s: clustered defects (G9), simulator vs exact model
python scripts/crossbar_study.py                   # about 20 s: crossbar cost and the K trade-off (G10)
python scripts/routing_defect_study.py             # about 12 s: the routing's own defects (G14)
python scripts/logic_style_study.py                # about 25 s: n-only versus complementary logic (G11)
python scripts/m6_campaign.py                      # about 6 min on 14 cores: seeds, K and the new fault classes (G15, G16)
```
At the commit this section was written, the six C++ test suites, the sixteen Python tests and rv64ui
+ rv64mi (65/65 at K = 8, 16 and 32) all passed locally on Windows with MinGW. The GitHub Actions
jobs were added along the way and their results have not been checked from here. (Checked on 2026-11-10 through
the public API: the six most recent runs, 2026-11-04 to 2026-11-06, all succeeded. The headline G19 and G20 results
are also regenerated by `make reproduce`; see `docs/audit-2026-11.md` for what was rerun that day and matched.)
