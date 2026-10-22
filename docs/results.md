# Results

Results come from scripts in this repository with fixed seeds. Every number is
a simulation under tier-C assumptions, not a measurement. Rejected ideas and
failures are recorded here, not deleted.

## M5: What was shown, what was not, and what would change it

Read this first; the milestone sections below it hold the evidence.

**Status of the evidence.** Everything here is simulation. The register file and ALU are models
beneath an unmodified golden RV64IMAFDC emulator. Of the 54 parameters in `config/params.toml`, 41
are tier C (hypothetical). The thirteen sourced ones (8 tier A, 5 tier B) are all from one paper: eight
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
| C13 | Costing the spare-routing crossbar changes the best register-file design and the K trade-off. Tagged routing costs about 10.8x the storage of the spares it serves; the cheapest silicon per working register file is 8 regions with 192 spares (4.21x the bare file per good file, against 7.11 for the 128-spare ideal pool and 1.54 if the crossbar were free). At chip level the register file is about 90% of the silicon, so ALU width moves silicon by at most 13% and speed decides: the widest sliced K (32) wins silicon x cycles, with 32 and 64 within 5% for a replicated ALU. A single reused ALU unit needs far less silicon than the replicated organisation that M2 and M3 modelled, at the same cycle count. The routing's own transistors are treated as fault-free; if any failure there were fatal, no spare design would yield (5.5e-83 for the cheapest). | G10: exact models, transistor-count cost model with five paper-anchored or derived constants | transistor count as area; tagged-array steering as the design; fault-free routing | a cheaper or wire-dominated routing layout; routing with its own defect tolerance (would make the numbers attainable); an ALU 16x an adder (single unit then prefers K = 16) |
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
jobs were added along the way and their results have not been checked from here.

## Checking p against the primary source (done after M5)

**What was checked, in order.** The paper is Ao, Zhou et al., "A RISC-V 32-bit microprocessor based on
two-dimensional semiconductors", *Nature* 640, 654-661 (2025), doi:10.1038/s41586-025-08759-9. I first
had only its Supplementary Information (SI), then its nine source-data archives, and finally its main
text (the PDF was supplied by the project owner). This section reflects all three and corrects what I
wrote when I had only the first. Quotations are short and the paper is not redistributed.

### What the main text says
- **Registers (page 659).** The yield of 8-bit registers, each with 144 transistors, "reached 71% at the
  wafer scale", and fell to "approximately 7%" for a 64-bit register with 1,152 transistors. The authors
  attribute the fall to the relatively low grade of their laboratory cleanroom and the stability of
  their processing tools, and say the process must be improved to an industrial standard. (The SI caption
  of Supplementary Fig. 7 says the 8-bit yield "can reach" 71%.)
- **Isolated devices (page 657).** Three batches of wafers, 7,000 transistors each, 800 randomly selected
  per test: "overall transistor yield reached 99.92%". A 30 x 30 array of 900 inverters in the
  6 mm x 6 mm region of the chip: 898 of 900 "worked correctly", printed as 99.77% (898/900 is 99.78%),
  with an overall noise tolerance of 0.5 V.
- **The design (Methods, Extended Data).** The processor is built on the open-source SERV bit-serial
  RISC-V CPU, with a **1-bit ALU**; a 32-bit operation takes 32 clock cycles, one bit per cycle. It has
  5,931 transistors (Extended Data Table 2), runs at 1 kHz and 4 V, and its 32-bit register is four 8-bit
  shift-register blocks of 144 transistors each, **the same block whose 71% yield was measured**.
- **What the paper does not report.** No yield for the processor itself (nor how many dies were tried
  or worked) and no radiation or upset data. The ">99.9%" yield I earlier flagged from a news summary as
  unverified appears to be the 99.92% isolated-transistor figure, not a processor yield.

### What the Supplementary Information adds
- A 4-inch wafer with 28 matrices of "64-bit" registers, each matrix being 8 parallel 8-bit shift
  registers: 224 8-bit registers.
- The wafer map in panel (c) shows one green or red stripe per register. Counting it two independent ways
  for the failures gives **59 failed of 224, so 165/224 = 73.7% work**, with all 28 matrices segmenting
  into exactly 8 registers. That is not the 71% in the text, and the paper does not explain the
  difference. Both numbers are kept. The transcribed map is `results/si_fig7_wafer_map.txt`, and
  `python -m radslice.wafer_map` reproduces the analysis from it.
- The map has 2 of its 28 matrices with no failed register (7.1%), where independent failures at the
  measured rate would give 2.4. That matches the text's "approximately 7%", so the 64-bit figure may well
  have been obtained this way; the text does not say.

### What this does to the project's premises
| Premise from the brief | What the source shows |
|---|---|
| 8-bit register yield about 71% | **Confirmed** (main text p. 659), for an 8-bit MoS2 *shift register* of 144 transistors; the map count is 73.7%. |
| 64-bit register yield about 7% | **Confirmed** (main text p. 659): "approximately 7%" for 1,152 transistors. The project's own 0.71^8 = 0.0646 agrees with it. (My earlier statement that the SI did not state this was true of the SI and wrong of the paper.) |
| Defects fall independently on 8-bit slices | **Supported at the register scale**, in two ways: the 8-bit and 64-bit yields imply the same per-transistor yield (next section), and the wafer map shows no clustering between matrices (variance of failures per matrix 0.91 against 1.55 for independent failures, so not over-dispersed; 18 adjacent failed pairs against 13.6 expected, P = 0.17; outer bands 26.6% failing against 26.0% in the middle). One wafer of 28 matrices has little power against mild clustering. |
| The fabricated chip resembles the modelled core | **No.** It is a 32-bit bit-serial design with a 1-bit ALU; the modelled core is a hypothetical 64-bit slice-serial design with K = 8, 16 or 32. |

**An unexplained oddity.** The first register of each matrix failed in 1 of 28 matrices, against 7.4
expected (P = 0.002 for that position; about 0.017 allowing for the eight positions examined); the other
seven positions fail at 21% to 46%. It may be a layout or test effect. I cannot explain it, and the paper
does not mention it.

### The central finding: register yield is not predicted by device-level yield
Per-transistor failure rate implied by each published figure, if transistors fail independently
(`python -m radslice.paper_data` reproduces this):

| source | failing transistors |
|---|---|
| isolated-transistor test (99.92%) | 0.080% |
| inverter test (2 of 900; an inverter has 2 transistors) | 0.11% (95% interval 0.03% to 0.41%) |
| 8-bit register (71% over 144 transistors) | 0.238% |
| 64-bit register (about 7% over 1,152 transistors) | 0.231% |

| circuit yield predicted by independent failures | 8-bit register | 64-bit register |
|---|---|---|
| from the transistor test (99.92%) | 0.891 | 0.398 |
| from the inverter test | 0.852 | 0.278 |
| **reported** | **0.71** | **about 0.07** |

- **The two register yields agree with each other.** Over an 8-fold range of size, 71% and about 7% imply
  the same per-transistor yield (0.99762 and 0.99769). Yield falls with transistor count the way an
  independent, area-proportional defect process predicts. That is direct support for the project's
  p-to-the-power model, and for scaling a slice's yield with its transistor count (the ALU's
  p^(K/8)) at least for register-like circuits.
- **But registers fail two to three times more often per transistor than isolated devices do.** Device
  yields would have given a 64-bit register about 40% (or 28%) where about 7% was measured. The loss
  therefore happens at circuit level (interconnect and vias across three metal layers, clocking,
  threshold-voltage variation, or the process drift the authors cite) and not in the transistors alone.
  The paper does not say which. The consequence for this project is that the circuit-level `p` it uses is
  the right quantity, and a device-level figure would have overstated yield considerably.
- **A whole-chip extrapolation, not a claim of the paper.** Applying the register defect density to all
  5,931 transistors gives a yield near 7.5e-7. The paper does not say how many dies were tried, so
  that cannot be compared with anything; it assumes the chip's logic has the same defect density as its
  shift registers, which is untested. It does show why redundancy matters at this scale.
- **The fabricated design is the extreme of the slice-width trade-off studied here.** Its ALU is 1 bit
  wide (K = 1) and a 32-bit operation takes 32 cycles. This project's K = 8, 16 and 32 sit between that
  and a full-width ALU. That is context, not a measurement of anything in the model.

### The paper's source-data files
The nine source-data archives (`MOESM2` to `MOESM10`; 52 spreadsheets) are the curves and waveforms
behind Figs 1 to 4, Extended Data Figs 2, 5 and 7, the process-step importance rankings and the 25-cell
library. **They contain no yield data**: no wafer map, no per-register pass/fail and nothing about
radiation. Supplementary Fig. 7 has no source-data file, so the register yield cannot be recomputed from
raw data.

`Fig2_f.xlsx` holds the 900-inverter measurement: **897 curves with data (893 distinct)**. The paper
counts 898 of 900 working. Using my own stricter definition of "working" (full swing: V_OH above 3.5 V and
V_OL below a threshold), `scripts/nature_source_data_check.py` finds 25 failures at a 0.3 V threshold, 7
at 0.5 V and 5 at 0.8 V or more, so **my definition is stricter than the paper's**, whose criterion is
not given beyond a 0.5 V noise tolerance. Of the paper's 900, three curves are missing from the file.
The per-transistor cross-check above therefore uses the paper's own count, not mine.

One pattern in the file is unexplained. In its column order, 5 of my 7 failures at 0.5 V fall within 21
consecutive curves (indices 866 to 886); placed at random that has probability about 0.0001. The array is
30 x 30, and if the columns run row by row those indices are the last two rows, which would be an edge
of the array. That is a hypothesis I cannot test, because the file's order is undocumented. If it holds,
it is an edge effect in a different, small-circuit dataset and does not change the register-scale
conclusions; if it is something else, it is unexplained.

### Effect on the parameters and conclusions
- The assumptions register now has **5 tier A, 2 tier B and 22 tier C** parameters. New tier A: the
  64-bit register (0.07), the isolated-transistor yield (0.9992) and the inverter yield (0.9977), and the
  8-bit figure now cites the main text. `yield.p_slice_good` stays at 0.71, tier B.
- **The equivalence assumption behind tier B is narrower than before but still real.** The measured block
  is the chip's own 8-bit register block, but a byte slice of the modelled register file is a
  random-access slice with read and write ports, not a shift register. The ALU's p^(K/8) is still an
  assumption, supported in form (area-proportional scaling) but not measured for ALU logic.
- **No result in this document changes**, because the value used (0.71) is unchanged. Sensitivity to the
  two readings of the measurement (closed form, ideal crossbar):

| | p = 0.71 (text) | p = 0.7366 (map count) |
|---|---|---|
| 64-bit register, no spares | 0.0646 | 0.0867 |
| register file, 112 spares | 0.7481 | 0.9658 |
| register file, 128 spares | 0.9718 | 0.9990 |
| spares for 99% (area) | 134 (1.52x) | 119 (1.47x) |
| ALU spares for 99%, K = 8 / 16 / 32 | 9 / 12 / 22 | 9 / 11 / 18 |

  The register-file cliff moves by about 15 spares, and "the shared pool works at about 1.5x" holds
  under both. The K ordering (finer slices need less spare area) is unchanged.
- **The M4 upset rates are not touched.** Nothing in the paper concerns radiation, so every upset rate
  stays tier C.

### Still unverified
How "works" was decided for a shift register (and so why the text says 71% and the map counts 73.7%);
whether the 64-bit figure was measured as matrices with all eight registers working; how many processor
dies were tested and how many worked; whether defects cluster at scales above a register; whether the
logic and interconnect of the chip have the register's defect density; and anything about radiation. The
paper contains none of these.

## G9: do the yield conclusions survive clustered defects? (done after the check of p)

Everything above assumed defects fall independently on slices. Here the **marginal slice yield is held
at exactly p = 0.71** and only the correlation between slices changes, so any difference is due to
clustering alone. The model is the standard negative-binomial one, generalised to blocks: each block of
slices draws a defect-rate multiplier from a gamma distribution (shape alpha; small alpha is strong
clustering, alpha tending to infinity is independence). It is computed exactly (numerical integration
and convolution, `python/radslice/cluster_model.py`) and simulated with the real self-test and remap
(`radslice_yield`); the two agree on 23 cases (largest |z| = 2.35). `python scripts/cluster_study.py`
reproduces every number here (about 35 s); the tables are in `results/cluster_study.md`.

### What the published wafer map does and does not constrain
- **Within a matrix.** Clustering stronger than alpha = 9.4 (pairwise failure correlation about 0.03) is
  rejected at about 95%. The map is, if anything, slightly under-dispersed.
- **Between neighbouring matrices.** No spatial structure (Moran's I = -0.08, P = 0.67 for positive
  autocorrelation).
- **Not constrained at all: a defect rate common to every matrix on the wafer.** Wafer-, batch- or
  die-level variation shifts all matrices together and leaves no difference between them, so one wafer
  cannot show it. The paper's datasheet does attribute spreads in some device parameters to "batch
  variations", and says the 8-bit yield "can reach" 71% in the caption, so a batch-to-batch component
  is plausible; its size is unknown.

### Verdicts on the claims
| # | Claim | Verdict | Numbers (256 primary slices, p = 0.71, ideal pool unless stated) |
|---|---|---|---|
| 1 | Clustering flattens the yield-versus-spares curve: a high target needs more spares, a low one fewer | **Confirmed, and the scale decides how much** | 99% needs 134 spares if independent; with the strength the map allows between matrices (alpha = 9.4): **147** if the clustering is in blocks of 32 slices, **248** (1.97x slices) if it is die-level. At 128 spares the yield falls from 0.972 to 0.717 (die-level). Below about 100 spares clustering helps (96 spares: 0.26 to 0.45). Under strong die-level clustering (alpha = 1) 99% cannot be reached with 640 spares. |
| 2 | A shared pool still beats private per-register spares | **Confirmed** | 4 private spares per register (32 regions): yield 0.0001 independent, below 0.12 under every clustering tested; the pool is 0.63 or better. |
| 3 | A spare pool of limited reach loses more than an ideal pool, and more under clustering | **Confirmed, and larger than expected: reach matters even with independent defects** | 128 spares in total: ideal pool 0.972; 2 regions 0.843; 4 regions 0.528; **8 regions 0.161**; 16 regions 0.013. With clustering confined to blocks the size of a region (alpha = 9.4) the same row reads 0.717, 0.512, 0.260, 0.066, 0.005. |
| 4 | Finer ALU slices still need less spare area (C6) | **Confirmed**, but absolute areas grow | 99% ALU yield needs 2.12x / 4.00x / 12.0x ALU area at K = 8 / 16 / 32 if independent; 2.50x / 5.25x / 21.0x at alpha = 9.4; 2.88x / 7.50x / 43.5x at alpha = 4. The ordering holds in every case. |
| 5 | TMR's yield penalty shrinks if the register-file copies share a die-level defect rate | **Not supported: the penalty gets larger** | 144 spares per copy: 1 copy 0.9990, 3 copies 0.9969 if independent (a 0.2-point cost); 0.8095 to 0.7453 at alpha = 9.4 (a 6.4-point cost, 3-copy/1-copy ratio 0.92). |

### Consequences for the earlier conclusions
- **C2 stands only as a best case.** "The shared pool works at about 1.5x" holds for independent defects
  and for clustering at scales the wafer map can test. If the defect rate varies from die to die with
  correlation as small as 0.03, 99% needs about 2x the slices, and spares cannot fix a bad die at all:
  under strong die-level variation the yield saturates well below 1. The result is also conditional on
  an ideal crossbar; the reach of the spare routing is a first-order design parameter (8 regions
  cost 81 points of yield at 128 spares).
- **C5** is unaffected in mechanism: the simulator, using the real self-test and remap, agrees with the exact
  clustered model. What changes is the yield the self-test recovers, not whether it recovers it.
- **C6** holds in order; **C11** is worse than stated (above).
- The headline spare area is therefore a range, not a number: about 1.5x if independent, about 1.6x
  with block-level clustering at the strength the data allow, about 2.0x with die-level variation at
  that same pairwise correlation, and more if it is stronger. Which applies is exactly what one wafer
  cannot say.

### Assumptions and limits
- Clustering is gamma-mixed block multipliers with independent blocks; no spatial gradient and no
  correlation that decays smoothly with distance (a real process may be in between a block and the die).
- Alpha below the data bound (4 and 1) are stress tests, not predictions. The die-level cases at the data
  bound are what the matrix-scale data permit, not what they indicate; they are worst cases for a
  constant correlation across the whole file.
- The 71% is one wafer; if it is a good wafer, the typical `p` is lower and the spare requirements above are
  optimistic as well.
- The hypothetical register file is far larger than anything the paper fabricated, so "die-level" here is
  a different physical scale from the paper's chip.

### Failures and corrections along the way
- My hypothesis about TMR (claim 5) was wrong, and is reported as such above.
- I first expected the *placement* of spares to matter for the ideal pool. It does not: only the total
  number of bad slices matters, so only how slices are grouped into blocks changes the result. Placement
  matters only for spares of limited reach (claim 3).
- A first reading of the wafer-map bound as covering die-level clustering was wrong: the test compares
  matrices with each other and so cannot see a rate they all share. The section above states what it
  constrains.
- Both the exact model and the C++ simulator passed their tests first time, which is when I distrust a
  test; mutants (a wrong calibration, a wrong yield threshold, ignoring spare regions, an unshared die)
  were each caught.

### What would falsify these conclusions
- Measured die-to-die or wafer-to-wafer yield variation, which would pin the scale and strength
  (per-wafer yields from the fabricating group would do).
- A spare-routing crossbar with a reach and cost different from both extremes studied.
- Defect correlation that decays with distance in a way these block models represent badly.

## G10: what the spare-routing crossbar costs (done after G9)

`python scripts/crossbar_study.py` (about 20 s, exact models, no simulation) writes
`results/crossbar_study.{csv,md,svg}`. It costs the routing that lets a spare stand in for a bad slice, in
transistors, with every published anchor from the Nature paper (144 T per 8-bit register block, 27 T per
1-bit adder with control, 27 T per four-input selection module) and every other choice a tier-C knob
(`config/params.toml`, `[cost.*]`; `python/radslice/cost_model.py`, 17 tests). p = 0.71 throughout.
Transistor count stands in for area; wiring and layout are not counted, so every cost below is more likely
too low than too high.

**What is costed.** Two ways to steer a spare into the register file. *Tagged*: spares sit in a side array,
each holding a tag naming the slice it replaces, every access is compared against all tags, and the output
lane multiplexers gain one input per spare that reaches the lane. *Per-position*: every one of the 256 slice
positions has its own multiplexer over the spares in reach, the naive crossbar; an upper bound. Spare *reach*
is the number of regions (1 = the ideal pool). For the ALU, two organisations, because M2 and M3 modelled
one of them and I had not said so: *replicated* (one physical unit per slice position, 64/K of them, plus
spares) and *single* (one K-bit unit reused for all 64/K positions, plus spares; this is what the fabricated
chip does at K = 1). The beat model is the same for both, so in this model the replicated organisation
costs more silicon for no gain in cycles.

### Verdicts on the four claims

| # | Claim | Verdict | Numbers |
|---|---|---|---|
| 1 | The ideal crossbar costs more than the slices it protects | **Confirmed, by a large factor** | Tagged routing is about 10.8x the storage of the spares it serves: 128 spares in one pool add 0.50 of a bare register file in storage and 5.41 in routing. Per-position routing for the same pool is 128x the bare file. |
| 2 | The cheapest design is not 128 spares in an ideal pool | **Confirmed** | Independent defects, tagged routing: the cheapest silicon per working file is **8 regions, 192 spares** (yield 0.929, silicon 3.91x the bare file, **4.21** per good file), against **7.11** for the 128-spare ideal pool and 1.54 if the crossbar were free. The crossbar multiplies the cost per good file by 2.7x at the optimum and 4.6x at the ideal pool. The optimum is broad: 4 regions 4.50, 16 regions 4.83. With per-position steering it moves to 32 regions (14.18). Reaching 99% yield: 8 regions, 224 spares, 4.43. With die-level clustering (alpha = 9.4): 8 regions, 224 spares, 5.10 against 9.44 for the pool; 99% needs 352 spares, 6.39. No sparing yields 8e-39. |
| 3 | The ordering of K changes once routing is costed | **Partly: the ordering by yield does not matter much; speed decides** | The register file is about 90% of chip silicon, so ALU width and organisation move the chip's silicon little: a single unit goes from 4.21 (K = 1) to 4.74 (K = 32), +13%; the replicated ALU is cheapest at K = 8 (4.86) and 5.29 to 5.40 at K = 1 and 32. Silicon times cycles is then decided by cycles: the widest sliced K (32) wins, K = 1 costs 14x more (alu_mix, base 2). K = 64, the unsliced ALU, needs 29 spare 64-bit units and costs 7.13 (+50% over the single unit at K = 32), so full width loses on silicon. |
| 4 | Conclusions depend on which tier-C assumption | **Mostly robust, with named exceptions (below)** | |

### Where the answers move (E4: one knob at a time)
- The register-file optimum stays at **8 regions, 192 to 200 spares** under every knob except the
  per-position topology (32 regions, 320 spares). Cost per good file ranges 3.30 (free tag compare) to 6.63
  (4x multiplexer cost) for the tagged design, 14.18 for per-position; never below 3.3, i.e. at least 2x the
  1.54 that a free crossbar would predict.
- K* by silicon x cycles is 32 for most settings. It becomes **64 for the replicated ALU** when multiplexers
  cost 2x or more, with the tag compare at 27 T, with 60,000 T of other silicon, with base = 1, or with
  per-position steering: the 32-to-64 difference is only 3% to 5% at the default (1.03 to 1.05 against 1.00), so "32 or 64" is the honest answer. It
  becomes **16 for the single-unit ALU** only if an ALU unit is 16x an adder (4x does not move it).
- The share of chip silicon in the ALU is 4% to 18% at the single-unit K*.

### E5: the routing is made of transistors, and this study still treats it as fault-free
The remap logic, tag table and multiplexers are transistors with the same defect rate as any other. With the
per-transistor yield implied by the register measurements (0.99762, tier B), a block of just 291 transistors
that must be perfect already fails half the time; with the isolated-device figure (0.9992, tier A), 866. The
cheapest register-file design above has 79,488 routing transistors (the ideal pool 199,296). If any single
failure there were fatal the chance they all work would be 5.5e-83 (2.4e-28 at the isolated-device rate).
The single-unit ALU's routing, 324 transistors at K = 8 with 4 spares, would survive 46% of the time.
So the cost-per-good numbers above are **lower bounds that assume routing that cannot fail**. A real design
needs defect tolerance in the routing itself (its own spares, or logic that degrades gracefully), and that is
not modelled; it is the largest open item from this study, and it applies to M3's remap logic as well.

### What this changes
- **C2** stays true as an in-model ordering (shared pool beats private spares) but 0.97 yield at 128 spares
  for 1.5x the slices is not a design: costed, the pool is 6.9x the bare file, and a limited-reach design is
  cheaper per good file.
- **C6**: the areas "2.1x / 4x / 12x ALU area at K = 8 / 16 / 32" are for the replicated organisation. For a
  single reused unit the joint chip optimum uses 4, 6 and 12 spare units at K = 8, 16, 32 (ALU yield 0.998,
  0.993, 0.978), which is 1,404, 3,942 and 14,796 transistors of ALU in all; at chip level the ALU is the
  smaller part of the cost.
- The M2 beat model gives the replicated organisation no speed advantage, so in this model it is dominated
  by the single unit. M2/M3 modelled the replicated one; that choice was an interpretation, not stated as one.

### Mistakes made on the way (kept)
- A first prototype optimised the ALU's spares on its own, minimising ALU silicon per good ALU. It picked no
  spares at K = 8 with 6.5% ALU yield, because a bad ALU also wastes the register file. The metric is silicon
  per good *chip*, optimising register file and ALU jointly; the prototype is not in the repository.
- A first register-file topology put a multiplexer at every slice position; that is the *per-position* upper
  bound above, 3x to 20x more expensive per good file than the tagged design that I should have started from.
- The search stops at 420 spares (360 under clustering); the 64-region rows hit that cap and are not minima.

### What would falsify this
- A routing layout whose cost is far below transistor-count-times-multiplexer (or a steering scheme cheaper
  than the tagged array): the optimum would move toward the ideal pool.
- Wire-dominated routing, which would raise every cost here and push the optimum toward finer regions.
- An ALU that is much larger than an adder (16x) with a single unit: K* moves to 16.
- Routing that cannot be made defect-tolerant at modest cost: no spare design would then work at this
  per-transistor yield, and the register file would need a different approach altogether.

## G14: do the spares and routing survive their own defects? (done after G10)

`python scripts/routing_defect_study.py` (about 12 s, exact models) writes `results/routing_defect_study.{csv,md,svg}`;
the model is `python/radslice/routing_model.py` (10 tests; with q = 1 it reproduces G10). Each spare carries its
private routing (tag, compare, multiplexer inputs), which fails with it, so a spare's yield is p x q^(its private
transistors); at the G10 optimum that is 414 transistors per spare. A shared block of 1,152 transistors (tier C) is
fatal unless replicated. q is the per-transistor yield: 0.9992 (isolated device, tier A) or 0.99762 (implied by the
register yields, tier B).

| # | Claim | Verdict | Numbers |
|---|---|---|---|
| 1 | A spare's own routing lowers its yield and raises the spares needed | **Confirmed** | Spare yield at the G10 design: 0.710 (fault-free routing), 0.510 (q = 0.9992), 0.265 (q = 0.99762). Cheapest register file, spares' private routing only: 8 regions in every case, 192 spares (4.21x the bare file per good file) becomes 280 spares (5.72x, 1.36x) at q = 0.9992 and 568 spares (10.58x, 2.52x) at q = 0.99762. |
| 2 | The shared block is a single point of failure that needs replicating | **Confirmed, and the cost depends on an assumption I made up** | One copy of the 1,152-transistor block yields 0.064 at q = 0.99762 (the same as a 64-bit register, 7%). With ideal selection among copies (optimistic) the design needs 8 copies at q = 0.9992 (6.10x per good file, 1.44x) and 48 at q = 0.99762 (12.75x, 3.01x). The block's size is tier C. |
| 3 | The cheapest design changes | **Partly: the region count does not, the spare count and cost do** | 8 regions throughout; spares 192 to 288 to 576; per-position steering (the naive topology) is hopeless at q = 0.9985 and below (no design above 50% yield within 1,200 spares). |
| ALU | Spares of the ALU carry routing too | **Single unit survives; replicated does not** | A single reused unit needs 2 to 16 spare units at q = 0.9992 and costs 6.10 to 6.81x per good chip across K = 1 to 32. The replicated ALU needs 34 to 36 spares at K = 8 to 32, and at K = 1 has only 26% ALU yield even with 60 spares at q = 0.9992; at q = 0.99762 its chip cost is 57x to 265x. K* by silicon x cycles stays 32 for the single unit. |

What this changes. C13's "if any failure there were fatal, no spare design would yield" was too pessimistic
because routing mostly disables a spare, not the chip; and not pessimistic enough, because it left out that the
spares then need to be many more. The conclusion for the register file is a range, 1.4x to 2.5x on top of G10, with a
further 7% to 21% for the shared block, depending on a per-transistor yield nobody has measured for routing. Independent
defects only; clustering of routing defects is not modelled.

**Mistakes on the way.** The first search compared cost per good in two different units and found a design
with 0.6% yield "cheapest"; a debugging print of the q = 1 case, which must reproduce G10, showed it. The
search ranges were too small (a result at the cap of 800 spares or 8 copies is a limit, not a minimum) and were widened
until the optimum moved off the cap. The q = 1 rows differ from G10 by 3% only in the version that includes the shared block.

## G15 and G16: more fault classes, and seeds and K (done after G14)

`python scripts/m6_campaign.py` (627 cells x 400 runs, about 6 minutes on 14 cores) writes `results/m6_cells.csv`,
`m6_summary.md` and `m6_pareto.svg`. Harness changes (all off by default; runs without them reproduce the M4 numbers
exactly): flips in the PC, in the instruction word being fetched, in five CSRs and in the memory the program touches;
multi-bit bursts; SECDED on 64-bit memory words; defects that appear after boot with an optional online self-test and
remap. Tests: 762 checks in `test_experiment`, 335,657 in `test_protected_regfile`; six mutants caught, two survived
(see mistakes). All rates are tier C.

### G16: seeds and K (part A of the campaign)
| Claim | Verdict | Numbers |
|---|---|---|
| The ranking of protection schemes survives 5 seeds and K = 8, 16, 32 | **Confirmed, with one pair that cannot be separated** | 14 of 15 pairs keep their order in all 5 seeds at every K; the other pair is register-file parity-dup against TMR, which succeed equally often at these rates (11.0% and 11.1% at K = 8; the ALU dominates the failures). |
| Success probabilities stay inside their intervals | **Not confirmed: the pooled intervals are too narrow, and the seed spread is the honest uncertainty** | Min to max over the 5 seeds is up to 6.6 points (ALU DWC at K = 16: 56.6% to 63.2%) against a pooled Wilson interval about 2.5 points wide. The cause is in the design: the three workloads share each seed's random streams, so their runs are correlated and the pooled sample is smaller than it looks. M4 gave every cell its own seed and is not affected. |
| K affects exposure | **New finding** | ALU duplication-with-comparison succeeds more at larger K (41.4%, 59.8%, 73.5% at K = 8, 16, 32) because fewer beats per operation means fewer chances for an ALU upset; the cost in cycles falls (1.85x, 1.74x, 1.59x). The full scheme (register-file TMR + scrub + ALU DWC) succeeds in every run at every K. |

### G15: the new fault classes (parts B1 to B5)
| # | Claim | Verdict | Numbers |
|---|---|---|---|
| 1 | Control-state faults dominate silent failures at equal per-bit rate | **Not supported as stated; the detail is more useful** | A PC flip fails the run 94% to 98% of the time, almost always as a crash or hang (silent corruption 0.7% to 2.5% of runs), an instruction-word flip 75% to 87% (of which 40% to 60% silent: 14.3% of runs at 1e-7), a CSR flip about 4% (the workloads run in machine mode and touch few CSR bits), a register-file bit flip 33% (M4 at 1e-9). The register file has 1,984 exposed bits against 64 + 32 + 320 here, so with the register file unprotected it still causes most failures; once it and the ALU are protected, control state is what is left (B5: 36% to 54% hang or crash). Voting control state removes all of it in this model; the voter is assumed fault-free. |
| 2 | Memory upsets matter, and SECDED removes single flips | **Confirmed** | On the program's footprint at 1e-10 per bit-cycle: 9.0% silent corruption and 4.8% crash without ECC, 0% with it for single flips (23% recovered by the check). Bursts of 2 become detected stops (14.8% fail-stop), bursts of 3 escape this worst-case SECDED (6.4% silent, 4.6% crash, against 7.7% and 5.7% raw). |
| 3 | A burst defeats single-error protection | **Confirmed** | Bursts of 2 in one register-file word: parity detects nothing (10.8% silent, 17.2% crash, same as unprotected); parity-dup sees the copies differ but cannot correct (78% fail-stop); TMR corrects bursts of 1, 2 and 4 because they hit one copy only. This assumes the copies are far enough apart that one event cannot reach two. |
| 4 | In-field defects need a periodic retest | **Not supported at the rates tried, and the cost matters more than the benefit** | TMR already masks defects that land in one copy: 99.6% success at 1e-7 per slice-cycle (about 12 defects per run) with no retest. A retest that stalls the machine for 8 cycles per slice per copy lowers success at higher rates (3e-7, about 36 defects per run: 96.5% without, 58.1% with a retest every 500 instructions) because the stall lengthens the run and defects arrive per cycle. A free retest helps slightly (99.2%). Without redundancy a retest cannot recover data already corrupted, but it turns silent corruption and crashes into detected stops (52% to 68% fail-stop at 3e-7 and 1e-6). |
| B5 | All classes together | **Complete protection works in this model** | With register-file TMR + scrub, ALU DWC, voted control state, SECDED memory with scrub and online retest, every run in 3 workloads x 400 completes correctly; each protection removed brings its failures back (without voted control state 36% to 54% hang or crash; without SECDED memory 5% to 11.5% hang or crash and 6.5% to 18.8% silent). This is a statement about the model, with its fault-free voters, ECC logic and retest logic, and rates set so that events happen. |

**Limits and mistakes (kept).**
- An early version stopped the machine on a double memory error the instant it happened and made ECC look worse than none
  (46% fail-stop with ECC against 0% without), because the harness counted upsets in memory nobody reads. Now a double
  error stops the machine only when its word is read, and upsets land on the memory the program touches (349, 253 and 123 words);
  an earlier rate that spread events over 4 MiB had made memory look harmless (the footprint is 0.02% to 0.07% of it).
- The first retest policy fail-stopped every scheme when a copy ran out of spares; TMR can mask, so it now carries on degraded.
- A mutant that skipped resetting the remap in the retest survived, as an equivalent mutant (permanent defects stay
  defects, so the rebuilt remap is the same). A mutant that does not restore the flipped instruction word after a fetch upset also survives; it is a
  known test gap (nothing observes memory after a run).
- The register file is the only place a defect can appear after boot, the retest assumes its own logic is fault-free, and
  the ECC syndrome logic and the voters are assumed fault-free.


## G11, G12, G13: logic style, process mechanisms and the space barriers (literature, done after G15)

**How this section was made.** G11 is answered from the paper itself (the PDF, main text and Methods) and a
computation. G12 and G13 are a literature survey done by web search; for most sources only the abstract or
a search excerpt was read, which is stated per row. No number from these sources is used as a model
parameter; they tell us which fault types exist and what scale the claims have. Where a search found
nothing, the table says so.

### G11: the paper's logic is n-type only, so every earlier cost already assumed that style
- **Claim 1 (what the paper's library is): confirmed from the paper.** "Our RV32-WUJI is based on the MoS2
  n-type metal-oxide-semiconductor architecture" and "the entire RV32-WUJI chip uses only n-type MoS2
  transistors" (main text, pp. 656-659). The basic gate is an enhancement-and-depletion-mode ratioed
  inverter: one driver and one load transistor; gates use only the driver network plus a single load
  (p. 657). It runs at 4 V, with a longest-path delay of 171 microseconds (a few kHz). The authors say pass
  transistor logic "may be a compromise" and that 2D CMOS is the long-term aim. So the 144, 27 and 27
  transistor counts behind G10 and G14 are n-only ratioed counts.
- **Claims 2 and 3: computed** (`scripts/logic_style_study.py`, `results/logic_style_study.md`). A multiplier
  mu on every transistor count (a complementary n-input gate is 2n transistors, an n-only ratioed gate is
  n + 1: ratio 4/3 for two inputs, 8/5 for four, towards 2) makes a block yield p^mu. At mu = 4/3, 8/5, 2
  the slice yield is 0.633, 0.578, 0.504. With fault-free routing the cheapest register file stays at **8
  regions** but needs more spares (192, 264, 320, 424), and the cost per good file rises from 4.21 to 5.29,
  6.24, 7.83 times the file's own bare silicon, which is 7.1x, 10.0x and 15.7x the n-only bare file in
  absolute silicon. With the isolated-device routing yield (0.9992) the figures are 5.72, 8.06, 10.41,
  14.95 (10.8x, 16.7x, 29.9x absolute); at the register-implied routing yield and mu = 2 no register file reaches even 1e-6 yield. The K
  ordering does not move: K = 32 minimises silicon x cycles in every row, with the single reused ALU unit
  always cheaper than the replicated ALU (and the replicated ALU out of its search range at mu = 2).
- **What this does and does not say.** A complementary design is not "worse by mu": it is mu times the
  transistors for the same function, at an unchanged per-transistor yield, which is the pessimistic case.
  p-type devices may have a different per-transistor yield (a further factor not modelled); a complementary
  gate has no static current while a ratioed gate does (not modelled); and the paper's note that threshold
  control of load and driver sets the inverter's switching point means that threshold drift can break a
  ratioed gate's noise margin, not only flip a stored bit (see G13, not modelled).

### G12 and G13: literature table
Rows say what a source reports, what it predicts for a fault model, and whether RadSlice models it. "Read"
says how much of the source was read.

| # | Topic | What the source reports | Read | Predicts | Modelled here? |
|---|---|---|---|---|---|
| 1 | Threshold-voltage spread, cause | In exfoliated devices a large device-to-device spread of threshold voltage is attributed to unclean interfaces, trapped substrate charge and sulfur vacancies; oxygen plasma moved the mean from -18.7 V to -0.9 V and reduced the spread ([arXiv 1607.07500](https://arxiv.org/pdf/1607.07500)). Adding or removing sulfur vacancies shifts threshold voltage in opposite directions ([Nanoscale 2015](https://www.doi.org/10.1039/C5NR00253B)). | abstracts | A gate whose load and driver thresholds drift out of match fails at the logic level (wrong output level), not necessarily as a stuck output bit | No: all permanent defects are stuck bits |
| 2 | Variability at wafer scale | Synthetic monolayer CVD MoS2: threshold variation consistent with charge density and trap variations as low as about 1e11 cm^-2 ([Smithe et al., Nano Lett. 2017](https://poplab.stanford.edu/pdfs/Smithe-LowVariabilityMoS2-nano17.pdf)); a 1300-device study says the effect of scaling on variability is not yet systematically known ([Nano-Micro Lett. review](https://www.ncbi.nlm.nih.gov/pmc/articles/PMC9849648/)) | abstracts | Variability can be low when the process is clean; the paper's 71% suggests the process behind it is not at that level | Partly: p is taken from the paper's measurement |
| 3 | Grain boundaries | Grain boundaries modify transport and the effect depends on boundary type and current direction (review excerpt). No source found that ties grain-boundary density to yield | search excerpt | Clustered failures if boundaries are unevenly spread, but no quantified link | G9 models clustering generically; no process link |
| 4 | Contacts | Contact resistance limits scaling; current crowding at the metal-TMD interface raises it further; doping that tunes threshold can leave devices hard to turn off ([Nano-Micro Lett. review](https://www.ncbi.nlm.nih.gov/pmc/articles/PMC9849648/), arXiv excerpt) | search excerpt | Slow or weakly driven gates: timing failures, not stuck bits | No: no timing faults |
| 5 | Encapsulation | Unencapsulated MoS2 FETs lose on-current and shift threshold with moisture and oxygen; a spin-coated HMDS layer kept mobility within 10% over 25 days ([arXiv 2403.05885](https://arxiv.org/pdf/2403.05885)); in one comparison an Al2O3 cap lowered yield by 20% against other caps (search excerpt, source not opened) | abstracts | Packaging and encapsulation are a yield input and a lifetime input, and the choice that protects may cost yield | No: sweepable as a multiplier on p, not done |
| 6 | The paper's own process | Threshold control "at wafer scale is crucial for achieving a high overall yield"; yield was improved by co-optimising the process with a machine-learning method (main text p. 656) | full text | Threshold control is the lever the authors name | See rows 1 and 2 |
| 7 | Total ionizing dose | Few-layer MoS2 FETs with polymer-electrolyte gates reported up to 3.75 Mrad(Si), recoverable by annealing at 100 C, against about 1 Mrad(Si) for earlier silicon and 2D rad-hard parts, by the same group ([arXiv 2110.02601](https://arxiv.org/pdf/2110.02601)) | abstract | A single-group claim on single transistors | No dose model |
| 8 | Gate dielectric under radiation | ZrO2 and h-BN gates show opposite threshold shifts under irradiation: the channel can survive while the gate stack drifts ([Wang et al., TNS 2019](https://resource.aminer.org/pub/5ce2d0b1ced107d4c63a8510/total-ionizing-dose-response-of-mos-transistors-with-zro-and-h-bn)) | abstract | Dose moves thresholds, which matters most for ratioed logic | No |
| 9 | Displacement damage | Monolayer FETs tolerated 1.56e13 MeV/g under 30 keV protons; the authors note few studies address displacement damage ([J. Semicond. 2024](https://www.jos.ac.cn/article/doi/10.1088/1674-4926/24090027)) | abstract | Permanent defects arrive with fluence | Yes, as the in-field defect class (G15), rate tier C |
| 10 | Heavy ions | An exfoliated single-layer MoS2 FET was destroyed at 4e11 ions/cm^2 of 1.14 GeV uranium while a graphene device survived ([arXiv 1304.3614](https://arxiv.org/pdf/1304.3614)) | abstract | Single-device destruction is possible at high LET and fluence | In-field defect class, rate tier C |
| 11 | Single-event upsets in 2D circuits | None found for MoS2 circuits or memory. The closest hit cites laser-induced single-event transients in black-phosphorus MOSFETs | search | Unknown | Upset rates are tier C because of this |
| 12 | Atomic oxygen, UV | In lubricant studies atomic oxygen oxidises the MoS2 surface with sulfur loss, confined to the top layer; sputtered films flown 43.5 h in low Earth orbit were oxidised ([Tribology Lett.](https://dx.doi.org/10.1007/s11249-004-8094-9) and a Shenzhou-7 exposure study, search excerpts). For a one-to-few-layer channel the top layer is the channel | excerpts | LEO needs encapsulation; deep space does not have atomic oxygen | No |
| 13 | Vacuum, thermal cycling | Nothing found for MoS2 electronics | search | Unknown | No |
| 14 | The lubricant record | Lubricity is a bulk or sputtered-film property, and the studies above are about friction and wear | excerpts | Says little about a transistor channel | n/a |
| 15 | p-type devices | MoS2 pins near the conduction band (n-type); WSe2 pins near mid-gap and is ambipolar, which spoils off-state behaviour; hole-injection contacts (MoOx, W1-xCrxSe2 alloy) and a hetero-channel WSe2 p / MoS2 n inverter with gain above 10 and about 7 pW static power are reported (search excerpts: [ACS AMI 2025](https://par.nsf.gov/biblio/10399172), [PMC 7047721](https://pmc.ncbi.nlm.nih.gov/articles/PMC7047721/), a dissertation) | excerpts | Complementary 2D logic exists as demonstrations, not as a manufacturable process | G11 with a multiplier |

### Verdicts
- **G11 claim 1 confirmed; claims 2 and 3 computed.** The conclusions about the register file (8 regions) and
  K are unchanged by the style; the numbers move a lot (above).
- **G12 claim 1 (mechanisms): answered with the table above.** The sources support sulfur vacancies and
  interface charge as the cause of threshold spread, and nothing in the survey ties a mechanism to
  clustering at a particular scale. **Claim 2 (is a stuck bit the right fault?): not supported.** Threshold
  and contact mechanisms predict logic-level and timing faults that the boot self-test as written would
  not find; this is a limit of the fault model, stated, not fixed. **Claim 3: yes**, encapsulation is named
  as a yield input, with a reported 20% yield effect in one comparison; not used as a parameter.
- **G13 claim 1 (separating what RadSlice can and cannot represent): done in the last column.** The ones it
  can represent are the in-field defect arrival (rows 9 and 10) and upset rates; the ones it cannot are
  threshold drift, dielectric charging, timing faults, atomic oxygen and p-type devices.
- **Limits of this survey.** Every row except row 6 (the paper itself) rests on an abstract or a search excerpt, not on a full reading;
  the radiation numbers come from three research groups; and absence of results for single-event effects
  or vacuum behaviour means "not found by these searches", not "does not exist".

## M0: closed-form yield model

Source: `python -m radslice.yield_model` (run from `python/`), `p = 0.71`
(tier C and unverified when this was run; since calibrated to the paper, which changes nothing here), 32 registers x 8 byte slices = 256 slices.

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

## M1: hooks in the golden emulator (riscv-emu, branch `radslice-hooks`)

The golden core is unchanged in behaviour. Two optional backends, both null by default, were
added: `RegFileBackend` (replacing the integer register file behind `read_reg`/`write_reg`) and
`AluBackend` (one entry point, `exec(op, a, b)`, covering the register-register and immediate
ALU forms, the comparisons behind branches, and data-address generation for loads, stores and
JALR). The PC adders, M/A/F/D and CSR operations are not routed through it.

- The brief expected 14 direct `regs[]` uses; there were 3 (two inside the accessors themselves
  and one in `dump_registers`). The real work was the ALU, which was inlined in about 25 places.
- Verification: `ctest` 16/16 before and 17/17 after (one new suite checking that every ALU form
  reaches the backend with the right operation and operands, and that a backend can steer
  results); the official riscv-tests 125/125 before and 125/125 after (3 rv64si tests do not
  assemble with this toolchain, and 2 rv64mi tests are excluded by the existing runner; all
  unchanged).
- Performance: on a fixed 300M-instruction loop, 12 alternating runs, the modified build took a
  median 0.976x the baseline's time. The run-to-run spread on this laptop was 2.2x, so this means
  no slowdown was detected, not that none exists below a few percent.
- Found, not fixed: `riscv_emu --dump` segfaults in the unmodified `main` build here.

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
