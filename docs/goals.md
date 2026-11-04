# Goals

## The goal
A radiation-tolerant inference engine for spacecraft, demonstrated on an FPGA and in simulation: an int8 MAC
array with a RISC-V controller that keeps giving correct classifications under manufacturing defects and
in-flight upsets, with the cost of that tolerance measured.

Why this target: Y Combinator's Summer 2026 Request for Startups reportedly asks for inference chips for
space, optimised for mass, thermal performance and radiation hardness. This is read from secondary coverage;
check ycombinator.com/rfs before quoting it. Its wording is not a specification, and RadSlice does not claim
to answer it. It decides what the project is for.

## What this project is not
- Not an MoS2 result. An FPGA is silicon. Nothing here shows what a 2D chip would do (G18 says the same).
- Not a throughput or power claim. MACs per cycle and per second are reported only if measured by a run in this
  repository; Quartus power numbers are estimates and labelled as such.
- Not novel in its parts. The study is how the parts behave together, with failures recorded.

## Milestones (each is a gap in `docs/gaps.md` with a falsifier; G19 is the current one)
| # | Milestone | Done when |
|---|---|---|
| 1 | G19 step 1: quantized network and integer reference | done: float 0.9704, integer 0.9685 held-out accuracy |
| 2 | MAC array in Verilog, bit-exact against the reference | done for int32 sums on random tiles and 20 test images (0 mismatches, four sizes); requantization and argmax not yet in hardware |
| 3 | Stuck-bit defects, boot self-test, spare rows or columns, remap | done in simulation: repaired arrays match the reference on 450 random chips, yield agrees with the closed form (|z| <= 1.2); the self-test is run by the testbench, not yet by a controller program |
| 4 | Upsets and protection schemes (checksum, parity, TMR), accuracy against fault rate | done in simulation: five schemes, accumulator and weight upsets, 2,700 images per cell (`results/g19_campaign.md`); triple voting tolerates the highest accumulator rate, the checksum lane is cheapest and the only one that catches weight upsets, but needs a clean weight copy |
| 5 | RV64 controller drives the array on the DE2-115 | fits, runs the network, MACs per second measured at the achieved clock |
| 6 | Fault injection on the board | the simulated result reproduced, or the disagreement recorded |
| 7 | Mission-control dashboard | shows real result files only, every number labelled simulated or measured |

## Stop or change direction if
- accuracy does not fall at any fault rate swept (then the experiment measures nothing; widen it first);
- the array is slower per watt-equivalent or per area than an unprotected one by more than the protection is
  worth at every rate that matters (record it as the result and say so);
- the controller adds nothing the array's own sequencer could not do (then drop the core from the pitch).

## What is not decided
Which orbit and shielding set the upset rate (all rates are swept, never derived), the mass and thermal
numbers the request cares about (not modelled), and whether a rad-tolerant control core is a gap that other
space-compute startups leave open (not checked).
