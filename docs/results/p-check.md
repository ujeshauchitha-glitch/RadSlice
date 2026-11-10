<!-- Split from docs/results.md on 2026-11-10 without changes to the text; the index of all result sections is docs/results.md. -->

## Checking p against the primary source (done after M5)

**What was checked, in order.** The paper is Ao, Zhou et al., "A RISC-V 32-bit microprocessor based on
two-dimensional semiconductors", *Nature* 640, 654-661 (2025), doi:10.1038/s41586-025-08759-9. I first
had only its Supplementary Information (SI), then its nine source-data archives, and finally its main
text (the PDF was supplied by the project owner). This section reflects all three and corrects what I
wrote when I had only the first. Quotations are short and the paper is not redistributed.

### What the main text says
- **Registers (page 659).** The yield of 8-bit registers, each with 144 transistors, "reached 71% at the
  wafer scale", and fell to "approximately 7%" for a 64-bit register with 1,152 transistors. The authors
  attribute the fall to the relatively low grade of their laboratory cleanroom and the stability of
  their processing tools, and say the process must be improved to an industrial standard. (The SI caption
  of Supplementary Fig. 7 says the 8-bit yield "can reach" 71%.)
- **Isolated devices (page 657).** Three batches of wafers, 7,000 transistors each, 800 randomly selected
  per test: "overall transistor yield reached 99.92%". A 30 x 30 array of 900 inverters in the
  6 mm x 6 mm region of the chip: 898 of 900 "worked correctly", printed as 99.77% (898/900 is 99.78%),
  with an overall noise tolerance of 0.5 V.
- **The design (Methods, Extended Data).** The processor is built on the open-source SERV bit-serial
  RISC-V CPU, with a **1-bit ALU**; a 32-bit operation takes 32 clock cycles, one bit per cycle. It has
  5,931 transistors (Extended Data Table 2), runs at 1 kHz and 4 V, and its 32-bit register is four 8-bit
  shift-register blocks of 144 transistors each, **the same block whose 71% yield was measured**.
- **What the paper does not report.** No yield for the processor itself (nor how many dies were tried
  or worked) and no radiation or upset data. The ">99.9%" yield I earlier flagged from a news summary as
  unverified appears to be the 99.92% isolated-transistor figure, not a processor yield.

### What the Supplementary Information adds
- A 4-inch wafer with 28 matrices of "64-bit" registers, each matrix being 8 parallel 8-bit shift
  registers: 224 8-bit registers.
- The wafer map in panel (c) shows one green or red stripe per register. Counting it two independent ways
  for the failures gives **59 failed of 224, so 165/224 = 73.7% work**, with all 28 matrices segmenting
  into exactly 8 registers. That is not the 71% in the text, and the paper does not explain the
  difference. Both numbers are kept. The transcribed map is `results/si_fig7_wafer_map.txt`, and
  `python -m radslice.wafer_map` reproduces the analysis from it.
- The map has 2 of its 28 matrices with no failed register (7.1%), where independent failures at the
  measured rate would give 2.4. That matches the text's "approximately 7%", so the 64-bit figure may well
  have been obtained this way; the text does not say.

### What this does to the project's premises
| Premise from the brief | What the source shows |
|---|---|
| 8-bit register yield about 71% | **Confirmed** (main text p. 659), for an 8-bit MoS2 *shift register* of 144 transistors; the map count is 73.7%. |
| 64-bit register yield about 7% | **Confirmed** (main text p. 659): "approximately 7%" for 1,152 transistors. The project's own 0.71^8 = 0.0646 agrees with it. (My earlier statement that the SI did not state this was true of the SI and wrong of the paper.) |
| Defects fall independently on 8-bit slices | **Supported at the register scale**, in two ways: the 8-bit and 64-bit yields imply the same per-transistor yield (next section), and the wafer map shows no clustering between matrices (variance of failures per matrix 0.91 against 1.55 for independent failures, so not over-dispersed; 18 adjacent failed pairs against 13.6 expected, P = 0.17; outer bands 26.6% failing against 26.0% in the middle). One wafer of 28 matrices has little power against mild clustering. |
| The fabricated chip resembles the modelled core | **No.** It is a 32-bit bit-serial design with a 1-bit ALU; the modelled core is a hypothetical 64-bit slice-serial design with K = 8, 16 or 32. |

**An unexplained oddity.** The first register of each matrix failed in 1 of 28 matrices, against 7.4
expected (P = 0.002 for that position; about 0.017 allowing for the eight positions examined); the other
seven positions fail at 21% to 46%. It may be a layout or test effect. I cannot explain it, and the paper
does not mention it.

### The central finding: register yield is not predicted by device-level yield
Per-transistor failure rate implied by each published figure, if transistors fail independently
(`python -m radslice.paper_data` reproduces this):

| source | failing transistors |
|---|---|
| isolated-transistor test (99.92%) | 0.080% |
| inverter test (2 of 900; an inverter has 2 transistors) | 0.11% (95% interval 0.03% to 0.41%) |
| 8-bit register (71% over 144 transistors) | 0.238% |
| 64-bit register (about 7% over 1,152 transistors) | 0.231% |

| circuit yield predicted by independent failures | 8-bit register | 64-bit register |
|---|---|---|
| from the transistor test (99.92%) | 0.891 | 0.398 |
| from the inverter test | 0.852 | 0.278 |
| **reported** | **0.71** | **about 0.07** |

- **The two register yields agree with each other.** Over an 8-fold range of size, 71% and about 7% imply
  the same per-transistor yield (0.99762 and 0.99769). Yield falls with transistor count the way an
  independent, area-proportional defect process predicts. That is direct support for the project's
  p-to-the-power model, and for scaling a slice's yield with its transistor count (the ALU's
  p^(K/8)) at least for register-like circuits.
- **But registers fail two to three times more often per transistor than isolated devices do.** Device
  yields would have given a 64-bit register about 40% (or 28%) where about 7% was measured. The loss
  therefore happens at circuit level (interconnect and vias across three metal layers, clocking,
  threshold-voltage variation, or the process drift the authors cite) and not in the transistors alone.
  The paper does not say which. The consequence for this project is that the circuit-level `p` it uses is
  the right quantity, and a device-level figure would have overstated yield considerably.
- **A whole-chip extrapolation, not a claim of the paper.** Applying the register defect density to all
  5,931 transistors gives a yield near 7.5e-7. The paper does not say how many dies were tried, so
  that cannot be compared with anything; it assumes the chip's logic has the same defect density as its
  shift registers, which is untested. It does show why redundancy matters at this scale.
- **The fabricated design is the extreme of the slice-width trade-off studied here.** Its ALU is 1 bit
  wide (K = 1) and a 32-bit operation takes 32 cycles. This project's K = 8, 16 and 32 sit between that
  and a full-width ALU. That is context, not a measurement of anything in the model.

### The paper's source-data files
The nine source-data archives (`MOESM2` to `MOESM10`; 52 spreadsheets) are the curves and waveforms
behind Figs 1 to 4, Extended Data Figs 2, 5 and 7, the process-step importance rankings and the 25-cell
library. **They contain no yield data**: no wafer map, no per-register pass/fail and nothing about
radiation. Supplementary Fig. 7 has no source-data file, so the register yield cannot be recomputed from
raw data.

`Fig2_f.xlsx` holds the 900-inverter measurement: **897 curves with data (893 distinct)**. The paper
counts 898 of 900 working. Using my own stricter definition of "working" (full swing: V_OH above 3.5 V and
V_OL below a threshold), `scripts/nature_source_data_check.py` finds 25 failures at a 0.3 V threshold, 7
at 0.5 V and 5 at 0.8 V or more, so **my definition is stricter than the paper's**, whose criterion is
not given beyond a 0.5 V noise tolerance. Of the paper's 900, three curves are missing from the file.
The per-transistor cross-check above therefore uses the paper's own count, not mine.

One pattern in the file is unexplained. In its column order, 5 of my 7 failures at 0.5 V fall within 21
consecutive curves (indices 866 to 886); placed at random that has probability about 0.0001. The array is
30 x 30, and if the columns run row by row those indices are the last two rows, which would be an edge
of the array. That is a hypothesis I cannot test, because the file's order is undocumented. If it holds,
it is an edge effect in a different, small-circuit dataset and does not change the register-scale
conclusions; if it is something else, it is unexplained.

### Effect on the parameters and conclusions
- The assumptions register now has **5 tier A, 2 tier B and 22 tier C** parameters. New tier A: the
  64-bit register (0.07), the isolated-transistor yield (0.9992) and the inverter yield (0.9977), and the
  8-bit figure now cites the main text. `yield.p_slice_good` stays at 0.71, tier B.
- **The equivalence assumption behind tier B is narrower than before but still real.** The measured block
  is the chip's own 8-bit register block, but a byte slice of the modelled register file is a
  random-access slice with read and write ports, not a shift register. The ALU's p^(K/8) is still an
  assumption, supported in form (area-proportional scaling) but not measured for ALU logic.
- **No result in this document changes**, because the value used (0.71) is unchanged. Sensitivity to the
  two readings of the measurement (closed form, ideal crossbar):

| | p = 0.71 (text) | p = 0.7366 (map count) |
|---|---|---|
| 64-bit register, no spares | 0.0646 | 0.0867 |
| register file, 112 spares | 0.7481 | 0.9658 |
| register file, 128 spares | 0.9718 | 0.9990 |
| spares for 99% (area) | 134 (1.52x) | 119 (1.47x) |
| ALU spares for 99%, K = 8 / 16 / 32 | 9 / 12 / 22 | 9 / 11 / 18 |

  The register-file cliff moves by about 15 spares, and "the shared pool works at about 1.5x" holds
  under both. The K ordering (finer slices need less spare area) is unchanged.
- **The M4 upset rates are not touched.** Nothing in the paper concerns radiation, so every upset rate
  stays tier C.

### Still unverified
How "works" was decided for a shift register (and so why the text says 71% and the map counts 73.7%);
whether the 64-bit figure was measured as matrices with all eight registers working; how many processor
dies were tested and how many worked; whether defects cluster at scales above a register; whether the
logic and interconnect of the chip have the register's defect density; and anything about radiation. The
paper contains none of these.
