# Gaps

Rule: before a feature is built, the gap it answers and what would prove it
wrong are written here. Nothing in this repository is a measurement. All device
and fault parameters are tier C (hypothetical) in `config/params.toml` until a
source is checked.

**Status.** Where each gap ended up after M5 is tabulated in `docs/results.md` (section "M5"),
and every assumption is listed in `docs/assumptions.md`.

## G1. Register-file yield collapses with width in 2D-material logic
**Claim to test.** If slice defects are independent with probability `p`, an
N-slice register has yield `p^N`, so a wide datapath is infeasible without
redundancy.
**Answered by.** M0 closed-form model; M3 Monte Carlo.
**Wrong if.** Defects are strongly clustered (independence then misstates the
loss in either direction), or the primary-source `p` differs enough to change
the conclusion. Update: `p = 0.71` has since been checked against the paper (main text and SI): 71% for an
8-bit MoS2 shift register (the wafer map counts 73.7%) and about 7% for a 64-bit register, which
imply the same per-transistor yield, as independent failures predict; the one wafer map shows no
clustering. The paper gives no chip-level yield and no radiation data. See `docs/results.md`; an
inverter dataset from the paper's source data shows one unexplained ordering pattern, so
independence above register scale is not settled.

## G2. Per-register spares versus a shared spare pool
**Claim to test.** A shared pool beats per-register spares at equal area.
**Answered by.** M0 table; M3.
**Wrong if.** Remap-table, mux area or delay (not modelled in M0) erases the
advantage, or defects cluster so that the pool is exhausted locally. M0 treats
the pool as an ideal crossbar, so its yields are upper bounds.

## G3. A slice-serial RV64I datapath stays functionally correct
**Claim to test.** A K-bit-slice ALU used 64/K times, handling carry, borrow,
sign, cross-slice shifts and W-form sign extension, is identical to a 64-bit ALU
for every RV64I ALU, compare and address-generation operation.
**Answered by.** M2 exhaustive and randomized tests; rv64ui and rv64mi.
**Wrong if.** Any op/operand pair differs from the golden ALU. One
counterexample suffices.
**Scope decision.** The PC adders (`pc + imm`, `pc + 4`) stay plain and are not
sliced; only data-side address generation uses the sliced ALU.

**M2 result (see results.md).** Tested against an independent reference at 8 and 16
bits (exhaustively over every 8-bit operand pair, densely at 16 bits), and densely at
64 bits for K = 8/16/32, then in lockstep with the golden core on random programs and
on rv64ui + rv64mi. "Exhaustive at 64 bits" is not claimed: it is infeasible, so 64-bit
correctness rests on boundary-value and seeded random coverage plus the structural
argument that the same slice logic is exhaustively correct at 8 bits.
**Also wrong if.** A bug exists that only shows at a width or slice count the small-width
tests do not reach (for example a carry that matters only across three or more slices).

## G4. Cost of slice-serial execution
**Claim to test.** K = 8, 16, 32 cost a quantifiable cycle overhead against a
full 64-bit datapath.
**Answered by.** M2 analytical beats-per-instruction-class model.
**Wrong if.** The beat counts disagree with a cycle-accurate RTL model. The
numbers are model estimates, never measurements. They depend on four tier-C
choices in params.toml: the non-ALU cost per instruction, one beat per slice op
regardless of K, free W-form sign extension, and a register-file port as wide as
the ALU slice. Keeping the PC adders plain also removes their cost from the sliced
side. Any of these changing materially changes the overhead.

## G5. A boot self-test can find and remap defective slices
**Claim to test.** A boot-time self-test over every physical slice (primaries and
spares) finds the defective ones, a remap onto good spares repairs the chip, and the
fraction of chips that end up repaired matches the closed form.
**Answered by.** M3: a simulator that samples defects, runs the real self-test and
remap on the real register-file and ALU models, and compares the pass rate with
`yield_model.py` within Monte Carlo error.
**Wrong if.**
- the simulated yield differs from the closed form by more than 4 standard errors
  (the closed form or the simulator is then wrong, and I say which);
- a chip that passes the self-test then computes a wrong result with hard defects
  (a test escape; with hard stuck-at defects there should be none);
- with intermittent ("weak") defects, the observed escape rate differs from the
  closed form (1 - q)^(2R) per defective slice (q: activation probability, R: test
  rounds), or cannot be pushed down by more rounds.
**Assumptions (mostly tier C, see params.toml).**
- Slices are defective independently, with probability 1 - p (p = 0.71 for an 8-bit
  register slice; tier B, calibrated to the measured yield of an 8-bit MoS2 shift register
  in the paper's Supplementary Information; the independence is not contradicted by the one
  published wafer map).
- A defective slice has exactly one stuck output bit or a stuck carry. Real defects
  are richer; this is the simplest fault that a test can be wrong about.
- An ALU slice of K bits has p_K = p^(K/8): the same defect density per bit.
- Spares are interchangeable with any slice (an ideal crossbar) and the remap table
  and its logic are defect-free, so yields are upper bounds. Clustered defects are
  not modelled in M3.
- The register file and ALU are repaired independently, each with its own spares, and
  chip yield is the product.

## G5a. Slice granularity and spare area
**Claim to test.** For equal spare *area*, finer ALU slices (K = 8) yield better than
coarser ones (K = 32), because a spare replaces less silicon.
**Answered by.** M3 closed form and simulation of ALU yield against spare area for
K = 8, 16, 32.
**Wrong if.** The curves cross or coincide once area is counted per bit, or a
modelled cost of finer slicing (more beats per op, see G4; more mux/remap hardware,
not modelled) outweighs the yield gain. M3 reports yield only; the beat cost is in
the M2 numbers, and the mux cost is unknown.

## G6. Combined yield and radiation handling on a slice-serial RV64 core
This is the research question. Spare slices exist in silicon, so no novelty is
claimed for sparing. The question is whether the 2D-specific fault model,
combined with yield sparing and transient-fault handling on a slice-serial
RV64 core, changes the outcome mix.
**Answered by.** M4 outcome classification across three workloads.
**Wrong if.** Silent-corruption and hang rates cannot be told apart from a
baseline with no 2D-specific assumptions, or the tier-C parameters dominate so
that no conclusion survives varying them.

### M4 refinement of G6: what can and cannot be answered
M4 has no sourced MoS2 radiation data, so the upset rate is a swept tier-C parameter
and every result is stated as a function of it. What M4 can answer is *structural*: how
the outcome mix (correct, detected-and-recovered, detected-but-not-recovered, silent
data corruption, hang/crash, boot failure) depends on the protection scheme, the
workload, and the upset rate, and what each scheme costs in model cycles and storage.
It cannot say whether MoS2 is more or less upset-prone than silicon; that needs
primary-source numbers.

## G8. Do the protection schemes reduce silent corruption, and at what cost?
**Claim to test.** Against an unprotected sliced core, (a) register-file parity with
duplication, (b) register-file triple modular redundancy (TMR), and (c) re-execution
or TMR of ALU operations each cut silent data corruption and hangs, and background
scrubbing stops single upsets from accumulating into uncorrectable ones.
**Answered by.** M4 injection campaigns: the golden core is the oracle, and every
run ends in exactly one outcome class, with Wilson confidence intervals.
**Wrong if.**
- the silent-corruption rate of a protected configuration is not lower than the
  unprotected one beyond the 95% intervals, at rates where the unprotected core does
  corrupt;
- a protection scheme creates a new failure mode that outweighs its benefit (for
  example parity-only scrubbing halting the machine on an upset in a register that was
  never going to be read again);
- the benefit disappears when the upset rate is swept, so it is an artefact of one
  chosen value.
**Assumptions (tier C).** Upsets are independent single-bit flips: in the register file
(stored data and parity bits, every copy) at a rate per bit per model cycle, and in an
ALU slice-unit output per beat. Memory and CSRs are fault-free. The control FSM is
triplicated and voted in the model but is not fault-injected, and the vote and
compare logic is fault-free. Detection and correction are instantaneous and free in
cycles, except that re-executing ALU operations really costs 2x or 3x the beats.
**Tradeoffs to report, not hide.** Storage (2x and 3x register copies), ALU beats
(2x and 3x), the added failure mode of fail-stop, and, once defects are combined, the
lower manufacturing yield of a design that needs several repaired register files.

## G9. Do the yield conclusions survive clustered defects?
Conclusions C1, C2, C5, C6 and C11 were all derived with defects falling independently on slices.
Real defects often cluster: the defect density varies from die to die, or from region to region
within a die. The study holds the **marginal** slice yield fixed at p (so average defect density
is unchanged, and any change is due to correlation alone) and varies how strongly, and at what
scale, slice failures are correlated.
**Claims to test.**
1. With clustering, the yield-versus-spares curve flattens (heavier tail of bad-slice counts), so a
   high yield target (99%) needs more spares than independence says, while a low target needs
   fewer. How much more, as a function of clustering strength?
2. A shared spare pool still beats per-register spares (C2).
3. A spare pool whose reach is limited to a region (the realistic case for spare routing) loses
   more under clustering than an ideal pool, because a cluster can exhaust the local spares; the
   loss depends on how the cluster size compares with the region size.
4. Finer ALU slices still need less spare area than coarser ones for the same yield (C6).
5. TMR's yield penalty (C11) shrinks if the three register-file copies share a die-level defect
   density, because their failures become positively correlated.
**Answered by.** A gamma-mixed defect model (the standard negative-binomial yield model, generalised
to blocks), computed exactly by convolution in Python, and simulated with the real self-test and
remap in C++, each checked against the other. The one published wafer map is used to bound how much
clustering the data allow at the scale of a register matrix.
**Wrong if.**
- the simulated and exact yields disagree beyond Monte Carlo error (one of them is then wrong);
- the shared pool loses to private per-register spares at any clustering strength swept (C2 fails);
- the K ordering of spare area reverses (C6 fails);
- the clustering the wafer map allows is already large enough to change a conclusion, so that the
  conclusion rested on an assumption the data do not support.
**Assumptions (tier C).** Clustering enters as a gamma-distributed multiplier on the defect rate of
each block of slices (shape alpha; small alpha is strong clustering; alpha tending to infinity
recovers independence); within a block, slices fail independently given the multiplier. No spatial
gradient across the wafer, and no clustering of the transient upsets of M4. The alpha values beyond
what the wafer map allows are hypothetical stress tests, not predictions.
**Result (see docs/results.md, G9).** Claims 1 to 4 confirmed; claim 5 not supported (TMR's yield
penalty grows when the copies share a die). The spare requirement depends on the *scale* of the
clustering: at the strength the wafer map allows between matrices, die-level variation raises the
spares for 99% yield from 134 to 248, block-level clustering only to 147, and one wafer cannot test
die-level variation. The simulator and the exact model agree on 23 cases.

## G10. What does the spare-routing crossbar cost, and does sparing still pay?
Every yield result so far (C2, C6, C12) assumed an ideal crossbar: any spare can replace any slice, at
no cost. That was flagged as the biggest unmodelled cost on the yield side. A spare is useless unless
a multiplexer can route it into the place of a bad slice, and the multiplexer is silicon too.
**Claims to test.**
1. At the published block sizes the multiplexing needed for an ideal crossbar costs more silicon than
   the slices it protects, by a large factor.
2. Once that is costed, the best register-file design is not "128 spares in an ideal pool": it is a
   smaller number of spares with limited reach, or possibly no sparing at all. The study finds the
   design with the lowest silicon per good register file and compares it with no spares.
3. The K comparison (C6, C4) changes once the ALU's routing is costed: finer slices have more
   positions and so more routing. The ordering of K by silicon per good ALU, and by silicon times
   cycles, is recomputed, including K = 1, 2 and 4 (the fabricated chip has a 1-bit ALU, K = 1).
4. Which tier-C cost assumptions the answers depend on, and where each flips the conclusion.
**Answered by.** A transistor-count cost model anchored on the paper's published block sizes (144
transistors per 8-bit register block, 27 for a 4-input selection module, 27 for a 1-bit adder with
control), the exact yield models of G9, and the M2 cycle model; swept over the uncertain choices.
**Wrong if.**
- the ideal crossbar turns out cheap relative to storage (claim 1 false, C2 unaffected);
- the cost-optimal design is still the large ideal pool (claim 2 false);
- the ordering of K does not change and is insensitive to every assumption (claim 3 is then a
  non-result, which would be recorded as such);
- the conclusions depend on one cost assumption in a way that makes them unusable (claim 4), in which
  case the honest answer is a range, not a number.
**Assumptions (tier C unless a published anchor is named).** Transistor count stands in for silicon
area, so wiring, vias and layout are ignored; a crossbar is wire-dominated, so this is likely to
UNDERSTATE its cost. A multiplexer of m inputs costs 9(m-1) transistors, derived from the published
27-transistor 4-input module (three 2-input multiplexers' worth). One mux per data bit per slice
position, no extra for read and write ports (swept). The cost of the remap table and of the
self-test logic is a swept tier-C parameter. Silicon outside the register file and the ALU is
ignored, so "relative cost" is for the parts that sparing affects.

## G11. Does the logic style (n-type only, or complementary) change the cost and yield picture?
MoS2 transistors are naturally n-type and good p-type 2D devices are hard to make, so the logic style of a
real MoS2 chip is a design decision, not a detail. Every transistor count used in G10 (144 per 8-bit
register block, 27 per adder bit, 27 per selection module) comes from the paper's own cell library and so
carries whatever logic style that library uses. The study has never said what that style is.
**Claims to test.**
1. The paper's own text and extended data say which logic style its cell library uses; this is read from
   the paper, not recalled.
2. If the style were changed to an n-only (pseudo-CMOS) one, each gate costs more transistors, so the
   register block, adder and multiplexer all grow; the G10 optimum (8 regions) and the K ordering are
   re-derived with the scaled counts and either hold or move.
3. Per-transistor yield is not independent of style: more transistors per function lowers function yield
   (the paper's 8-bit and 64-bit yields imply one per-transistor yield), so a bigger style costs yield as
   well as area. The size of that second effect is computed, not assumed.
**Answered by.** Reading the paper; a style multiplier on every transistor count in `cost_model.py`;
function yield re-derived from the implied per-transistor yield; the G10 study re-run across the multiplier.
**Wrong if.** The paper's library is already n-only or complementary in the style being compared, so there
is nothing to change; or the conclusions are insensitive to any multiplier in a plausible range, in which
case the result is "insensitive", recorded as such.
**Assumptions.** The multiplier is tier C unless a source gives the transistor count of the alternative
cell; any source used is named. Function-level timing and power (pseudo-CMOS is slower and burns static
power) are not modelled.

## G12. Which manufacturing-process mechanisms support the defect model used here?
The study uses one permanent-defect model (a slice is good with probability p, independently or clustered;
a defective slice has one stuck output bit) and one yield number (0.71 for an 8-bit register). The process
literature names several different mechanisms (grain boundaries, contact variability, threshold-voltage
spread, growth and transfer damage, encapsulation effects), and each predicts a different structure.
**Claims to test.**
1. Which mechanisms are reported for wafer-scale MoS2, with sources, and which of them predict clustering,
   a die-to-die rate change, a gradient across the wafer, or a failure that is not a stuck bit.
2. Whether the stuck-one-bit fault is the right fault type: a mechanism that makes a transistor slow or
   leaky rather than dead would produce faults the boot self-test as written cannot find.
3. Whether any published process choice has a yield cost large enough to be a design input (for example an
   encapsulation layer that lowers yield), and what the same layer does to radiation tolerance.
**Answered by.** A literature table in `docs/results.md` (each row: mechanism, source, what it predicts,
whether RadSlice models it) and, where a prediction is quantitative and sourced, a tier-B/C parameter and a
model extension.
**Wrong if.** The literature found does not distinguish between mechanisms at all (then the table says
"not distinguishable from these sources" and the defect model is unchanged).

## G13. What stands between MoS2 and use in space, and which of those can this model represent?
MoS2 is a spacecraft lubricant, but that is a coating property; a transistor needs a near-perfect one- to
few-layer film, so the lubricant record says little about electronics. The barriers named in the
literature are: total ionizing dose in the gate stack (threshold drift), displacement damage, heavy ions,
single-event upsets, vacuum, atomic oxygen and thermal cycling, process variability, and the missing p-type
device.
**Claims to test.**
1. The literature table (sources named) separates barriers that RadSlice can turn into a fault type or a
   parameter from those that need devices and beam time.
2. For the ones it can: a dose-dependent upset rate and in-field permanent defects (defects that arrive
   after the boot self-test) change the result of the protection schemes. Boot-time sparing alone cannot
   repair them, so the test is whether periodic re-test and remap restore correctness and at what cost.
3. At rates inside the ranges reported in the sources, which barrier first breaks correct execution
   (a ranking, not a probability).
**Answered by.** The G15 fault classes (in-field defects, dose scaling) in the M4 harness; sweeps.
**Wrong if.** The sources do not give enough to place any rate in a range (then every rate stays tier C and
the ranking is recorded as conditional on rates that are guesses).
**Assumptions.** Device-level radiation results (single transistors) transfer to circuit-level behaviour
only through tier-C factors; the model has no explicit gate-dielectric physics.

## G14. Do the spares and routing survive their own defects?
G10 costed the routing but left it fault-free, while its transistors fail at the same per-transistor rate as
any other: 79,000 routing transistors in the cheapest register-file design.
**Claims to test.**
1. A spare is a slice plus its private routing (tag, compare, multiplexer inputs). The routing of a spare
   fails with that spare, so a spare's own yield is p times the routing yield, and the number of spares
   needed rises. The effect is large: about 414 private transistors per spare at the G10 optimum lowers a
   spare's yield from 0.71 to about 0.27 at the register-implied per-transistor yield.
2. The routing shared by all spares (hit combine, final lane select, remap-table control) is a single point
   of failure unless replicated; its size is small, but the replication needed has a cost.
3. With both effects counted, the cheapest design differs from G10's (fewer regions? more spares?), and may
   not exist at the register-implied per-transistor yield.
**Answered by.** The exact model with unequal primary and spare yields, plus a shared-logic factor, and the
G10 cost search repeated for the two per-transistor yields (register-implied, isolated-device).
**Wrong if.** The changes to the optimum are within a few percent (then G10 stands and this is recorded as a
check passed).
**Assumptions.** Which transistors are spare-private and which shared is a design choice made explicit in
the cost model (tier C); independent defects (clustering of routing is not modelled).

## G15. More fault classes: multi-bit upsets, control state, memory, in-field defects
M4 injects single-bit upsets into register-file bits and ALU output bits only. A flip in the PC, a fetched
instruction, a CSR, or memory is not covered, and neither are multi-bit events or defects that arrive after
manufacturing.
**Claims to test.**
1. Faults in control state (PC, instruction fetch) dominate silent failures at equal per-bit rate, because
   a flipped PC or opcode usually crashes rather than computes a wrong value.
2. Memory upsets matter if memory is not protected: a bit flip in code or data persists until overwritten.
   A SECDED code over 64-bit words removes single flips; double flips become detected stops.
3. A multi-bit burst defeats single-error protection (parity, TMR with a shared event, SECDED); the burst
   size at which each scheme stops working is found by sweep.
4. In-field stuck-at defects (dose-induced, tier C) cannot be repaired by the boot self-test; a periodic
   re-test and remap is needed, and the harness finds how often.
**Answered by.** New injectors in `experiment.cpp` with deterministic tests per injector, then a campaign
across K and several seeds (M6).
**Wrong if.** Control-state faults turn out to be a small share of failures at equal rate (claim 1 false;
recorded), or bursts change nothing (claim 3 false).
**Assumptions.** Rates are per bit per model cycle and tier C; the control-state bit counts (64 PC bits, 32
instruction bits) are fixed by the architecture; CSR coverage is limited to those the workloads use.

## G16. Are the M4 conclusions robust to seeds, and to K?
M4 used one seed per cell and K = 16. Wilson intervals cover the sampling noise within a seed, not the
variation from one seed to another, and K affects both cycle cost and fault exposure.
**Claims to test.** Rerunning with several independent seeds and K in {8, 16, 32} leaves the ranking of
protection schemes unchanged and the success probabilities within their stated intervals.
**Answered by.** The M6 campaign with the cycle-overhead and success probability plotted together.
**Wrong if.** A ranking flips between seeds, or between K values, at a rate where the intervals say it
should not (then the M4 conclusion is withdrawn or restricted).

## G17. Does a VLIW organisation with four parallel units buy speed, and what does it do to yield?
The slice-serial design pays cycles for fewer transistors (about 1.6x to 4x at K = 32 to 8 in the M2 model). A
VLIW-style organisation fetches one wide word that bundles several operations and runs them on parallel units;
the compiler, not the hardware, decides what runs together. The proposal here: four parallel K-bit units, whose
purpose is speed, and which are also the spares, so that a unit found dead at the boot self-test costs width
instead of rejecting the chip.
**Claims to test.**
1. Four units speed up the real workloads, but by much less than 4x, because a trace of dependent operations
   cannot be spread over four units. The speedup is measured on the three workloads and the riscv-tests with
   the dependences read from the instructions, and compared for K = 8, 16, 32.
2. Because the units are also spares, a defective unit lowers the width (4, 3, 2 or 1 units) rather than
   rejecting the chip. Against one active unit plus three dedicated spares (all the same silicon), the
   expected throughput per fabricated chip is higher, and against four units that must all work (no sparing),
   the fraction of chips that work is higher. The comparison is expected throughput per unit of silicon.
3. The register file is what makes width expensive: every extra unit needs two read ports and one write port,
   and the spare-routing cost of G10 and G14 grows with the number of ports, so the register file may cost more
   than the units gain. The study finds where that crossover is, as a function of the per-transistor yield of
   routing.
4. Which tier-C assumptions flip the answer: how many operations the compiler can bundle (list scheduling
   within a basic block, with and without reordering), the cost of the front end per bundle, the port count
   per unit, and the yield per unit.
**Answered by.** A static scheduler over the real instruction traces (dependences, memory order, branches end
a block), producing cycles for 1 to 4 units; the exact distribution of good units from the per-unit yield;
the cost model with ports scaled by width; the joint figure of merit expected throughput per silicon.
**Wrong if.**
- the speedup on the real workloads is close to 4x (then the dependence structure is no obstacle, and the
  claim that it is would be false);
- the expected throughput of "four units that are also spares" is not above one active plus three spares
  (then the dual use of the units buys nothing);
- the register file ports cost makes the best design a single unit under every assumption tried (claim 3
  then settles the question the other way and is reported as such).
**Assumptions (tier C).** Static scheduling of the dynamic trace (the compiler is assumed to see the path that
was taken, an optimistic bound on what it can do from code alone); true data dependences, register
anti- and output dependences and memory order are kept, with no register renaming or memory disambiguation;
one load/store and one branch resource per bundle; the front end issues one bundle per `base` cycles, as the
M2 model charges `base` cycles per instruction; units are the single-unit K-bit organisation of G10.

## G18. Can the design run as real hardware on a student-priced FPGA, and does it match the golden model?
The study so far is software. An FPGA cannot show MoS2 behaviour (it is silicon), so it cannot answer any device
question. What it can show is that the architecture (slice-serial datapath, spare units, remap, parity, voting,
the VLIW issue of G17) is implementable and cycle-exact against the golden model, and it makes fault injection
fast and visible.
**Target.** Terasic DE2-115 (Intel Cyclone IV E, about 114,000 logic elements), built with Quartus Prime Lite.
The Verilog is kept board-independent (one wrapper per board); an Arty A7-100T is the fallback and needs only a
new wrapper and constraints. Simulation and synthesis checks use Verilator, Icarus Verilog and Yosys.
**Claims to test.**
1. A slice-serial RV64I core with K = 8/16/32 and the same retire order as the golden core passes the riscv-tests
   in simulation, and an instruction-by-instruction comparison with the golden model shows no difference.
2. The hardware beat count per instruction equals the model's (`SlicedAlu::beats_for`), so the cycle estimates of
   M2 are what the hardware does, not a separate story.
3. The core and its spares fit the DE2-115 with room for the fault-injection registers.
**Wrong if.** the hardware and the model disagree on any instruction result or on beats (then the model, not the
hardware, is corrected and the earlier cycle numbers are re-run); or the design does not fit.
**Not claimed.** Anything about MoS2, radiation or yield. An FPGA run is not a measurement of the chip.
**Order.** (1) one slice unit and the slice-serial ALU, exhaustively checked against `alu_reference`; (2) the
register file with parity and remap; (3) the in-order core, lockstep with the golden model; (4) spares and the
boot self-test; (5) the wide issue of G17; (6) the board wrapper and fault-injection switches.

## G7. A fair baseline
**Claim to test.** Comparing against a fault-free silicon register file with
MoS2 logic is needed to avoid overstating the benefit.
**Answered by.** The hybrid backend option in M2 and M4.
**Wrong if.** The hybrid baseline wins on every metric that matters. That is a
result and gets recorded in `docs/results.md`.

## Known limits (v0)
- Modelled datapath is RV64I + Zicsr. The golden core stays RV64IMAFDC.
- Memory, CSRs, the PC and the fetched instruction can be fault-injected since M6 (G15) but are fault-free
  in the M4 campaigns. The control FSM is described as triplicated; "voted control state" is the injected
  form of that. Voters, ECC logic, parity logic and retest logic are assumed fault-free.
- The register file and ALU are replaced beneath the golden core, so S/U modes
  and the MMU still run in the golden model. Only the register file and ALU
  are sliced. Bare-metal M-mode applies to the M3/M4 workloads.
- Single hart. No correlated multi-bit upsets beyond what params.toml states.
