<!-- Split from docs/results.md on 2026-11-10 without changes to the text; the index of all result sections is docs/results.md. -->

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
