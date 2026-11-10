<!-- Split from docs/results.md on 2026-11-10 without changes to the text; the index of all result sections is docs/results.md. -->

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
