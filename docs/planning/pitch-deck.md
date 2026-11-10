# Pitch deck: slide-by-slide, with what to say

> **Superseded on 2026-11-06.** The two council reviews (`council-verdict.md`, `council-2-direction.md`) rejected the
> spacecraft framing this deck uses (slides 1 to 3). Kept as a record; the numbers on slides 5 to 8 are still those in
> `results/`, but do not present the deck as written. The current one-sentence question is at the top of `README.md`.

Eleven slides plus a Q&A page. Every number below comes from a run in this repository (file named under each slide).
Anything not measured is marked, and the last section lists what never to claim. Total talk: about 10 minutes.

Audience assumed: people who could provide equipment or lab access (a university lab, a funder, a hardware sponsor).
Tone: confident about what is built and verified, plain about what is simulated.

---

## Slide 1: Title
**On the slide:** "RadSlice: measuring how cheaply a small AI engine can survive space radiation"
Your name, one line: "An open, reproducible test bench for fault-tolerant inference hardware."

**Say:** "I build and verify hardware that keeps working when radiation breaks parts of it. I'll show what exists, what it
shows, and the two things I need to turn simulation into measurement."

---

## Slide 2: The problem
**On the slide:** Three short lines.
- Radiation flips bits and slowly damages transistors in orbit.
- Chips built to survive it are expensive and old; ordinary AI chips are not built for it.
- Space-based computing is being attempted anyway.

**Say:** "Spacecraft that want to run AI on board face a choice. Radiation-hardened chips tend to lag commercial ones by
several generations and cost far more. Commercial chips are fast but fragile. The question is how much protection a
small inference engine really needs, and what that protection costs."

**Source care:** the "lag" and "cost" claims come from secondary web coverage I read, not from a measurement of mine. Say
"reported", or leave the multiplier out. Do not put a precise multiple on the slide.

---

## Slide 3: Why now
**On the slide:** "Orbital compute is attracting serious money and attention."
- Starcloud (a YC company building data centers in orbit) reportedly raised $170M in March 2026.
- YC's Summer 2026 Request for Startups reportedly includes inference chips for space: low mass, good thermals,
  radiation hardness.

**Say:** "I'm not claiming to answer that request. I'm saying there is demand for knowing how to make small AI hardware
survive, and that a measurement platform for it is useful."

**Before you present:** open ycombinator.com/rfs and confirm the wording. I read it second-hand, so do not quote it until
you have seen the page.

---

## Slide 4: The idea, in one picture
**On the slide:** Three boxes, left to right: **Spares + remap**, **Boot self-test**, **Checks during the run**.
- A few extra compute elements; a map says which element serves which job.
- At power-on, test every element; route work around broken ones.
- While running, checksums or voting catch bit flips; the engine re-runs or votes the answer.

**Say:** "None of these ideas is new on its own, and I don't claim they are. What is missing is a combined, reproducible
measurement of how they behave on an AI workload: accuracy against fault rate, and the cost of each option."

---

## Slide 5: What I built
**On the slide:** Block diagram: RISC-V controller core, weight memory, MAC array (8 lanes + spare elements), remap table,
fault-injection hooks. Footnote: "Verilog; checked bit for bit against software."
- Controller: small RV64 core, matches a reference emulator instruction by instruction (two programs, at slice widths K = 8, 16 and 32).
- Compute: int8 multiply-accumulate array with spares and a lane-to-element map.
- Test target: a 64-32-10 digit classifier, 96.85% accuracy fault-free (`results/g19_baseline.md`).

**Say:** "The controller part also compiles for a student-priced FPGA board: 16% of the chip, timing met at 6.25 MHz. It has
not run on the board yet."

(`docs/fpga-board.md`; the 16% and 6.25 MHz figures are from a Quartus compile, not a board run.)

---

## Slide 6: Result 1, it computes correctly
**On the slide:**
- Array matches an independent integer reference: **0 mismatches** over 1,760 random and real-network tiles at 4 array sizes.
- Deliberately breaking an element is caught every time (the check can fail).
- **Run it live:** `bash scripts/run-mac-tests.sh` (about 20 s).

**Say:** "Before measuring anything about faults, I proved the fault-free hardware is exact. A test that cannot fail proves
nothing, so each check has a control that must fail, and does."

(Source: `docs/results.md`, "G19 step 2". 420 + 420 + 520 + 400 = 1,760 tiles.)

---

## Slide 7: Result 2, broken parts are found and routed around
**On the slide:** Small table.

| Per-element good probability | Extra elements for 99% yield (8 lanes) | (512 lanes) |
|---|---|---|
| 0.99 | 1.12x | 1.02x |
| 0.9 | 1.50x | 1.15x |
| 0.7 | 2.25x | 1.51x |

- Self-test + remap repaired every accepted chip on **450 random defective chips**: 0 wrong results; accept/reject
  decisions correct on all 450.
- The yield agrees with the closed-form model (within 1.2 standard errors).

**Say:** "Wider arrays need proportionally fewer spares. The per-element defect rate decides everything, and that is a
number I do not have: it is exactly what real hardware data would supply."

(Source: `results/g19_yield.md`; defect rates are assumptions, labelled tier C. The overhead counts elements, not the
routing that connects them.)

---

## Slide 8: Result 3, protection against radiation upsets (the main slide)
**On the slide:** The figure `results/g19_campaign.png`, large. Caption: "Simulated; fault rates are assumptions; accuracy
counts a detected failure as no answer."

Three takeaways, one line each:
1. **Triple voting** (3x elements) tolerates the highest rate of upsets in the accumulators: 100x the unprotected array (within the rates swept).
2. A **checksum lane** (about 1.125x elements) gives the same protection as duplicate-and-compare (2x) at moderate rates,
   with almost no silent wrong answers. It breaks down at high rates because retries fail too.
3. For **corrupted stored weights**, voting and duplication do nothing; the checksum catches every case but needs a clean
   copy of the weights to restore.

**Say:** "This is the result I'd show a skeptic. It says protection is not one-size-fits-all: the cheapest option is not the
strongest, and which one you pick depends on where upsets hit. It also shows my own hypothesis that the checksum would win
everywhere was wrong; triple voting goes further."

**Numbers if asked:** accumulator floor (accuracy within 1 point of fault-free): none 1e-5, duplicate+retry 1e-4,
checksum 1e-4, triple vote 1e-3 upsets per element per cycle. 2,700 images per point.

---

## Slide 9: What this does not show (say it before they ask)
**On the slide:**
- It is simulation and FPGA, in silicon. It says nothing about MoS2 or other 2D materials.
- Every fault rate is an assumption, swept, never measured.
- The protection logic itself (voters, checksum adders) is assumed fault-free.
- Small network (2,368 weights). No power, thermal or mass figures.
- Not novel in its parts: spare elements and checksum-protected arrays are established.

**Say:** "I'd rather you hear the limits from me. They are also the reason I'm here: each one needs hardware or data I
don't have."

---

## Slide 10: Roadmap
**On the slide:** Four steps, with status.
1. Quantized network, exact reference: **done**
2. MAC array, bit-exact: **done**
3. Defects, self-test, remap, yield: **done in simulation**
4. Upsets and protection, accuracy vs rate: **done in simulation**
5. RISC-V controller drives the array on the FPGA board: **next**
6. Fault injection on the board; compare with simulation: **next**
7. Mission-control dashboard (real result files only): **next**

**Say:** "Four of seven steps are finished and reproducible. The remaining three need the hardware."

---

## Slide 11: The ask
**On the slide:**
1. **FPGA board time** (or a second board and a USB-Blaster cable) to run steps 5 and 6.
2. **Access to a radiation test source** (proton or heavy-ion beam time through a university or national facility) to replace
   assumed upset rates with measured ones.
3. **Any 2D-material device or wafer data** from a partner lab, if available.

**Say:** "With the first, I can show your people the system running with faults injected. With the second, the curves on slide 8
stop being assumptions. With the third, I can say something about new materials, which today I can't."

**Be ready to name** what you would do with each: a one-line deliverable per item (for example: "a recorded board run with
faults injected and the same accuracy-vs-rate figure, measured").

---

## Q&A page: hard questions and honest answers

- **"Is this a chip?"** No. It is a verified design and a simulation test bench, with a controller core that compiles for an FPGA.
- **"How does it compare to existing rad-hard chips?"** I have not compared. I have not measured speed, power or area
  against anything, and I would not claim it.
- **"Are your fault rates realistic?"** They are assumptions. The sweep shows where each scheme stops working, so a real
  measured rate can be read off the curves. Measuring that rate is what beam time is for.
- **"Why not just use a commercial radiation-tolerant FPGA?"** Fair point. Those exist, and this does not claim to beat them.
  What this offers is a transparent way to measure which protection is worth its cost on an AI workload.
- **"What about MoS2?"** The earlier part of the project studies it, but the evidence is one paper and assumptions. Nothing in
  the new work depends on it or says anything about it.
- **"Is the accuracy figure meaningful?"** It is 96.85% on a standard small digits set. It shows the method, not a useful
  product. A real application would use a bigger network.
- **"How do I know the results are real?"** Everything is reproducible with the scripts in the repository, every check has a
  control that must fail, and the model is checked against separately written code.
- **"What did you get wrong along the way?"** Examples I can name: I assumed the checksum scheme would beat triple voting and
  it did not; an earlier conclusion that the widest slice width was best flipped to a narrower one once I measured the real cycle cost
  on the Verilog core; an earlier estimate of the FPGA fit was about 2x too high. `docs/results.md` keeps these on purpose.

## What never to say
- That it beats anything, or that it is "the" solution to the YC request.
- Any speed (TOPS), power or energy number. None has been measured.
- That YC asked for this: you read the request second-hand.
- That it works for MoS2 or 2D chips.
- That the fault rates are real.

## Demo options (pick one, 2 minutes)
- **Fastest:** show the figure on slide 8, then run `bash scripts/run-mac-selftest.sh` (about 1 minute) and point at the line
  "accepted … rejected … all match".
- **With the board** (once connected and programmed): flip a bit with the switches and show the parity counter, as in
  `docs/fpga-board.md`.
