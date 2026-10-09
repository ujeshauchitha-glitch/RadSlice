# Gaps

Rule: before a feature is built, the gap it answers and what would prove it
wrong are written here. Nothing in this repository is a measurement. All device
and fault parameters are tier C (hypothetical) in `config/params.toml` until a
source is checked.

## G1. Register-file yield collapses with width in 2D-material logic
**Claim to test.** If slice defects are independent with probability `p`, an
N-slice register has yield `p^N`, so a wide datapath is infeasible without
redundancy.
**Answered by.** M0 closed-form model; M3 Monte Carlo.
**Wrong if.** Defects are strongly clustered (independence then misstates the
loss in either direction), or the primary-source `p` differs enough to change
the conclusion. `p = 0.71` is an unverified secondary figure.

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

## G4. Cost of slice-serial execution
**Claim to test.** K = 8, 16, 32 cost a quantifiable cycle overhead against a
full 64-bit datapath.
**Answered by.** M2 analytical beats-per-instruction-class model.
**Wrong if.** The beat counts disagree with a cycle-accurate RTL model. The
numbers are model estimates, never measurements.

## G5. A boot self-test can find and remap defective slices
**Claim to test.** Self-test plus remap recovers yield to the closed-form
prediction.
**Answered by.** M3.
**Wrong if.** Simulated yield differs from the closed form beyond Monte Carlo
error, or defects escape the test and corrupt results.

## G6. Combined yield and radiation handling on a slice-serial RV64 core
This is the research question. Spare slices exist in silicon, so no novelty is
claimed for sparing. The question is whether the 2D-specific fault model,
combined with yield sparing and transient-fault handling on a slice-serial
RV64 core, changes the outcome mix.
**Answered by.** M4 outcome classification across three workloads.
**Wrong if.** Silent-corruption and hang rates cannot be told apart from a
baseline with no 2D-specific assumptions, or the tier-C parameters dominate so
that no conclusion survives varying them.

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
