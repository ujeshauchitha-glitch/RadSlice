<!-- Split from docs/results.md on 2026-11-10 without changes to the text; the index of all result sections is docs/results.md. -->

## G19, step 2: the MAC array matches the integer reference

`rtl/mac_pe.v` is one int8 x int8 into int32 multiply-accumulate element with a stuck-bit mask (applied to the
stored accumulator, so the wrong bit feeds back) and a one-cycle flip hook. `rtl/mac_array.v` is P logical lanes
served by P + S physical elements through a lane-to-element map; weights are routed to the mapped element and
its accumulator is read back as the lane's result (an ideal any-to-any crossbar; its own cost and defects are
not modelled). `bash scripts/run-mac-tests.sh` simulates it in Icarus against sums computed in exact integers
by `scripts/g19_mac_vectors.py`, using random tiles (a fifth of them at the int8 extremes) and the real
network's layer 1 and layer 2 on 20 held-out images.

| Array (lanes + spares) | Tiles | Mismatches |
|---|---|---|
| 8 + 0 | 420 | 0 |
| 8 + 4 | 420 | 0 |
| 4 + 2 | 520 | 0 |
| 10 + 6 | 400 | 0 |

In every run with spares the lane-to-element map is rotated by a different offset per tile and every element no
lane uses is stuck at all ones: nothing leaked from an unused element and no weight was mis-routed. The check
can fail: with one used element given a stuck bit, every spare configuration reports mismatches (420 of 420,
520 of 520, 400 of 400 tiles).

**Not shown.** No defect has been repaired yet: that needs the boot self-test that finds which element is
defective and picks the map (step 3). Only int32 sums are checked, not the requantization between layers or
the argmax (those stay in software for now). No timing, area or fit figures exist for the array.
Each tile was run with a fresh clear, so accumulator state carried across tiles is not tested.

## G19, step 3: defects, boot self-test and remap on the MAC array

`bash scripts/run-mac-selftest.sh` runs `rtl/tb/tb_mac_selftest.v` on 150 random defective chips at each of three
sizes (8+4, 8+2 and 4+4 lanes+spares; defect probability per element 0.25, 0.15, 0.30; one stuck bit per
defective element). The testbench plays the boot controller: it tests every element with a two-read test
(clear, add 1*0 and read, add -1*1 and read: a bit stuck at 1 shows on the first read, stuck at 0 on the second),
keeps the first P healthy elements as the lane map, rejects the chip if fewer than P are healthy, and then runs
tiles. Expected values come from `scripts/g19_array_model.py`, a separately written model of the same rules.

- **Stuck-bit semantics:** with the identity map, the defective arrays' outputs equal the model's (0 mismatches).
- **Decisions:** the accept/reject decision equals "defective elements <= S" on every chip (0 mismatches;
  121, 122 and 140 of 150 chips accepted at the three sizes).
- **Repair:** every accepted chip reproduces the exact sums on all test tiles (0 mismatches).
- **Flip hook:** a single accumulator-bit flip at a chosen step gives the model's result (0 mismatches).
- **The check can fail:** with the stuck-at-0 half of the self-test removed, the same run reports 27 wrong
  decisions and 521 wrong results.
- **Yield:** the accepted counts agree with the closed form (z = -1.20, -0.22, -0.46); see `results/g19_yield.md`.
  Spares needed for 99% yield fall as a fraction as the array gets wider and rise as the per-element good
  probability g drops (for g = 0.9: 1.50x elements at 8 lanes, 1.15x at 512). g is tier C, so these are
  consequences of a model, not predictions for a chip.

**Not shown.** One stuck bit per element: multi-bit, weak (intermittent) and multiplier-internal defects are not
modelled, and a weak defect would pass this self-test (compare C7). The self-test is run by the testbench; no
controller program does it yet. The crossbar that makes any element serve any lane is ideal here: its cost and
its own defects are not modelled (compare G10 and G14), and the 2% to 125% overhead figures count elements only,
not routing.

## G19, step 4: upsets and protection, measured as accuracy

`python scripts/g19_campaign.py` then `python scripts/g19_report.py` (about 30 s). The real digit classifier runs on
an 8-lane array (320 array steps per image) with upsets, for five schemes; each cell is 2,700 images (the 540
held-out images with fresh faults, five times). Full tables: `results/g19_campaign.md`; figure:
`results/g19_campaign.png`. Before any number was kept, every scheme reproduced the integer reference exactly with
no faults, and 200 forced single flips agreed with the scalar model that the Verilog matches. Rates are tier C and
swept. An accuracy floor of fault-free minus one point (0.9585) is used for "still works".

**Accumulator upsets** (per element per cycle):

| Scheme | Elements | Highest rate at or above the floor | Silent wrong answers | Cycles at that rate |
|---|---|---|---|---|
| none | 1x | 1e-5 | from 7e-4 at 1e-5, 11% at 1e-3 | 1.00 |
| duplicate + compare + retry | 2x | 1e-4 | none observed up to 1e-2 | 1.09 |
| checksum lane + retry | 1.125x lanes, one wider lane | 1e-4 | none up to 3e-4; a few per 10,000 at 1e-3 | 1.05 |
| triple vote | 3x | 1e-3 | 0.3% at 1e-3, 26% at 1e-2 | 1.00 |

- Triple voting tolerates a rate 10x above the retry schemes, at 3x the elements.
- Both retry schemes turn silent errors into detected failures but stop working when a pass almost always sees an
  upset (the retries fail too): past 1e-3 they deliver almost no answers (accuracy 0.25 for duplicate and compare,
  0.66 for checksum at 1e-3).
- The checksum lane matches duplicate-and-compare at the floor for 1.125x rather than 2x of the elements, but it
  is not perfect: at 1e-3 it let a few answers through wrongly (two upsets whose effects cancel, or one in the
  checksum lane itself that happens to agree).
- A hypothesis stated before the run, that checksum protection would dominate, is not supported: triple voting
  reaches a higher rate.

**Weight upsets** (per stored bit per image, one draw per pass; the weight memory is shared by all copies):
- The unprotected network is robust: accuracy stays within a point of fault-free up to 1e-3 per bit (about 19
  flipped bits per image across the 18,944 weight bits), then falls to 0.952, 0.856 and 0.656 at 3e-3, 1e-2
  and 3e-2. Silent wrong answers appear well before that: 0.1% at 1e-4 and 1.8% at 1e-3.
- Duplicate-and-compare and triple voting on the accumulators do nothing for weight upsets (all copies share the
  corrupted weights): their silent-wrong curves equal the unprotected one.
- The checksum lane detects every weight upset (no silent errors at any rate) and, **if a clean master copy of
  the weights exists to reload from**, restores full accuracy at 2x the cycles at the highest rates. Without that
  clean copy it is the worst scheme (accuracy 0.84 at 1e-5, 0 above 1e-3), because retrying reuses the corrupted
  weights. The reload result rests on the master copy being immune and on the weights being corrupted once per
  pass, not again after the reload; real memories upset continuously, so it is an upper bound.

**Not shown.** Voters, comparators, checksum adders, the reload path and the controller's requantization are
assumed fault-free; single-bit flips only, independent per element and per cycle (no bursts or common-cause
events); a digit classifier of 2,368 weights, not an inference workload people use; no area or timing of the
protection logic (a 16-bit-weight checksum lane is counted as one lane); no power. The 2,700-image cells have a
standard error of about 0.3 points near 0.97 accuracy, larger in the steep part of each curve.
