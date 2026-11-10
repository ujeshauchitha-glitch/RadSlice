<!-- Split from docs/results.md on 2026-11-10 without changes to the text; the index of all result sections is docs/results.md. -->

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
