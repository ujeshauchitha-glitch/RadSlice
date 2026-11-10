# Track B: what RadSlice offers a 2D-materials group, and what it needs back

Status: a proposal to send, not an agreement. Nothing here claims that any 2D circuit is radiation resistant.

## The offer in two sentences
Groups that make 2D-semiconductor circuits (for example the MoS2 RISC-V work that this project's yield numbers come from) know how
their devices fail but rarely have a tool to say how much spare capacity and protection a processor built from them would need.
RadSlice is an open, reproducible simulation study (G1 to G20) of exactly that, in which every defect and upset rate is currently a
labelled placeholder that measured data would replace.

## What exists and can be shown today
- A yield model validated against a simulator, calibrated to the published per-register yields (8-bit about 71%, 64-bit about 7%).
- Spare-slice sharing, remap and a boot self-test; riscv-tests pass on defective simulated chips after repair.
- A fault-injection harness (register, ALU, PC, instruction, CSR and memory flips; bursts; in-field defects) and protection
  schemes with their cost.
- A verified Verilog core and a verified MAC array with spares, plus a preregistered weight-protection study (G20) that reports its
  wrong predictions.
- A table of which assumptions the conclusions rest on (`docs/assumptions.md`): 54 parameters, 8 sourced from one paper, 41 assumed.

## What the group would need to provide (any subset helps)
Each item replaces an assumption; the study then reruns with the measured value and the results are reported with and without it.

| Measurement | What it replaces | Why it matters |
|---|---|---|
| Yield of a circuit (not a single device) across several wafers or dies, and how "working" was decided | die-to-die variation of the defect rate (the one thing a single wafer cannot test); the stuck-bit-per-slice model | decides how many spare slices a register file needs (1.5x vs about 2x in G9) |
| Failure modes seen in failing circuits: stuck, slow, threshold-shifted, intermittent | the one-stuck-bit assumption; weak defects escape a short boot test (G5) | a self-test only repairs what it can see |
| Threshold-voltage and contact-resistance drift under total dose | no model of drift exists in RadSlice | the likely dominant degradation, and it produces logic and timing faults, not bit flips |
| Any single-event data for 2D circuits: upsets per bit per particle fluence, and what kind of upset | every upset rate in G4 and G15 | no such data was found in the literature check (G13) |
| Yield of the routing and select logic separately from storage cells | the routing treated as fault-free or given a placeholder yield (G14) | spare routing is the largest cost in the shared-spare design |

## What the group gets
- A quantitative answer to "if our process has these measured defect and upset rates, how much protection does a processor need and
  what does it cost?", with the code and every assumption open.
- Co-authorship on anything that uses their data, and their data never leaves their control unless they release it.
- A negative-results record: where the earlier conclusions changed once better inputs arrived (the K = 32 conclusion became K = 16;
  the yield of the shared spare pool depends on a die-level variation that one wafer cannot measure).

## What this must not claim
That MoS2 circuits are radiation resistant (device-level reports of high total-dose and displacement-damage tolerance exist, but
circuit-level and single-event evidence was not found); that the numbers predict any orbit; novelty in spares, parity, TMR or
fault-tolerant arrays (all established).

## First email (short; send only after a sponsor has seen it)
Subject: Open simulation tool for sizing spare capacity and protection in 2D-semiconductor processors

Hello, I am an undergraduate working on an open, reproducible study of how much redundancy a processor built from 2D-semiconductor
circuits would need, using the published register yields from your group's MoS2 work as the only calibration. Every other defect
and upset rate in the study is an explicit placeholder. If you have measured circuit-level yield variation, failure modes of failing
circuits, or any irradiation data, I would like to rerun the study with those values and share what changes, with you as
co-author on anything that uses your data. A two-page summary and the repository are attached. Would a 20-minute call be possible?

(Add the faculty sponsor's name and your own. Do not send before a sponsor has agreed; an unsponsored cold email to a research
group rarely helps and cannot be unsent.)
