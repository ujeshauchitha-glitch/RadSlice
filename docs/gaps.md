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
the conclusion. Update: `p = 0.71` has since been checked against the paper's Supplementary
Information (an 8-bit MoS2 shift register; the map count gives 73.7%), and the one wafer map
shows no clustering; see `docs/results.md`. The main text remains unread. An inverter dataset from the same paper's source data shows an
unexplained ordering pattern among its failures (undocumented column order), so independence is
not settled.

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

## G7. A fair baseline
**Claim to test.** Comparing against a fault-free silicon register file with
MoS2 logic is needed to avoid overstating the benefit.
**Answered by.** The hybrid backend option in M2 and M4.
**Wrong if.** The hybrid baseline wins on every metric that matters. That is a
result and gets recorded in `docs/results.md`.

## Known limits (v0)
- Modelled datapath is RV64I + Zicsr. The golden core stays RV64IMAFDC.
- Memory and CSRs are assumed fault-free. The control FSM is triplicated in the
  model but not fault-injected.
- The register file and ALU are replaced beneath the golden core, so S/U modes
  and the MMU still run in the golden model. Only the register file and ALU
  are sliced. Bare-metal M-mode applies to the M3/M4 workloads.
- Single hart. No correlated multi-bit upsets beyond what params.toml states.
