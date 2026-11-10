<!-- Split from docs/results.md on 2026-11-10 without changes to the text; the index of all result sections is docs/results.md. -->

## G20: weight-memory protection (preregistered; 3 of 6 predictions held)

Model and predictions were committed first (`docs/preregistration-g20.md`, commit 4e8daa4). `python scripts/g20_weight_protection.py`
(about 20 s); full table in `results/g20_weight_protection.md`. Persistent weight upsets at rate mu per stored bit per image, 2,700
images per cell, an exact SECDED (13,8) code checked to correct every single error and detect every double error. Floor: accuracy 0.9585.

| Prediction | Verdict | What happened |
|---|---|---|
| P1 unprotected, never scrubbed, fails the floor at mu >= 1e-5 | held | 0.961 at 1e-6, 0.846 at 1e-5, 0.333 at 1e-4 |
| P2 ECC alone holds the floor at 1e-6 | **wrong** | accuracy 0.663: errors accumulate over a 2,700-image mission, and one uncorrectable word anywhere among 2,368 fails the whole image (31.6% of images at 1e-6) |
| P3 ECC + scrub every 10 images holds the floor up to 1e-4 | **wrong** | holds only to 1e-5 (0.968); at 1e-4 accuracy is 0.894 with 7.6% detected failures, for the same reason |
| P4 checksum + reload: silent errors below 0.1%, reloads near 100% at 1e-3 | held | silent 0.000 at every rate; reload rate 1.5%, 17.9%, 84.7%, 100% at 1e-6 to 1e-3 |
| P5 ECC produces silent errors at the highest rates (3+ errors in a word, miscorrected) | **wrong** | none observed anywhere: a miscorrected word almost always sits in an image that also has a detected double-error word, which fails or reloads the image first |
| P6 scrub alone with S = 10 tolerates a rate at least 10x higher than S = 100 | held | floor holds to 1e-4 at S = 10 and to 1e-5 at S = 100 (exactly one decade; the grid cannot resolve more) |

**What the results say (in this model).**
- Plain scrubbing is a strong baseline: S = 10 holds the floor to 1e-4 with no extra memory.
- ECC without a restore is the worst of the protected options, because it only corrects what has not yet accumulated, and detection
  turns into failure. With a clean copy to reload from, ECC gives the full fault-free accuracy at every rate.
- Checksum plus reload also keeps full accuracy with no silent errors, but reloads often: it cannot correct, so any flip among 18,944
  bits (17.9% of images at 1e-5) causes a reload. ECC plus reload reloads far less often at the same rate (5.6% at 1e-5), at 1.625x
  the weight memory. This comparison is not like for like (ECC's exposure window is S = 100 images, the checksum's is one image),
  so it shows a trade between reload frequency and memory, not a winner.
- The cost of a reload (latency, energy, access to the clean copy) is not modelled and is the quantity that decides the trade.

**Not shown.** Independent images (consecutive images share their weight state in reality); a clean copy that cannot itself be
upset; fault-free ECC, checksum and reload logic (the hardware study is where that is removed); an ECC policy that rewrites on read,
which would change P2 and P3; any rate measured rather than swept. The checks of related work listed in
`docs/planning/council-2-direction.md` are still outstanding, so no novelty is claimed.
