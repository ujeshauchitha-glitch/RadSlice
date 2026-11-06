# G20 preregistration: weight-memory protection, written before the run

Date written: 2026-11-06. Committed before `scripts/g20_weight_protection.py` was run for the first time; the git history is the
timestamp. Results go in `results/g20_weight_protection.md` and are compared with this file line by line, including every
prediction that turns out wrong.

## Question
For a small int8 classifier whose weights sit in memory that suffers persistent bit upsets, how do these protections compare
in accuracy, silent wrong answers, detected failures and reload cost: nothing; periodic scrubbing (restore from a clean copy every S
images); SECDED ECC on each 8-bit weight word (13-bit codeword) alone; ECC plus scrubbing; ECC with reload from a clean
copy when it detects an uncorrectable word; and the G19 checksum-plus-reload (checked every image)?

## Model (fixed before the run)
- The 64-32-10 int8 digit classifier of `results/g19_network.npz`; 2,700 images (the 540 held-out images five times).
- Weight bit upsets are independent, rate mu per stored bit per image. A bit that has been exposed for t images since the last
  restore is flipped with probability (1 - (1 - 2 mu)^t) / 2. Image i of the run has t = 1 + (i mod S) for scrub period S, and
  t = 1 + i when never scrubbed (a mission of 2,700 images). The exposure of different images is treated as independent draws,
  which ignores the correlation between consecutive images.
- ECC is an extended Hamming (13,8) SECDED code on each weight byte; check bits can be upset too; correction happens on read
  without writing back. Single errors are corrected, double errors detected, three or more may be miscorrected.
- Checksum: the sum of each 8-row group's weights per column (as in G19), compared against the clean sums; any difference
  triggers a reload from a clean copy (assumed immune), so its exposure is one image.
- Accumulator and datapath faults are off. The checker, voter and reload logic are assumed fault-free in this study (the
  hardware study is where that assumption is removed). Rates are tier C and swept; nothing here is a prediction for orbit.
- Fault-free accuracy 0.9685; "still works" means accuracy of at least 0.9585 (one point lower), as in G19.

## Predictions
- **P1.** Unprotected, never scrubbed: accuracy falls below the floor at mu = 1e-5 and above.
- **P2.** ECC alone (never scrubbed) holds the floor at mu = 1e-6 but not at mu >= 1e-5, because errors accumulate past one per word.
- **P3.** ECC plus scrubbing every S = 10 images holds the floor at mu <= 1e-4.
- **P4.** Checksum plus reload gives a silent wrong answer rate below 0.1% at every mu swept (only flips that cancel in a column sum
  escape), and its reload rate approaches 100% at mu = 1e-3.
- **P5.** At the highest rates, ECC produces a nonzero silent-wrong rate from words with three or more errors being miscorrected.
- **P6.** Scrubbing alone with S = 10 tolerates a rate at least 10x higher than scrubbing alone with S = 100.

## What would make the study uninteresting
If every scheme holds the floor over the whole sweep, or none does anywhere, the rates are wrong for the question and the sweep is
widened before any conclusion is drawn.
