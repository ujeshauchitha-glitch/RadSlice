# Results

Results come from scripts in this repository with fixed seeds. Every number is
a simulation under tier-C assumptions, not a measurement. Rejected ideas and
failures are recorded here, not deleted.

## M0: closed-form yield model

Source: `python -m radslice.yield_model` (run from `python/`), `p = 0.71`
(tier C, unverified secondary figure), 32 registers x 8 byte slices = 256 slices.

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
