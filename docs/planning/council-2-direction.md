# Council 2: a better direction, and what makes it fundable

Same four role-played positions as `council-verdict.md` (Advocate, Skeptic, Investor, Pragmatist), two more rounds, this time with web
search allowed. **Limit:** the four share one model and one briefing, and the web results were mostly snippets, not full papers.
Treat the prior-art points as leads to check, not as a literature review.

## The direction
**One question, answered with a three-way cross-check.**

> For a small int8 classifier on an FPGA, how well do the ways of protecting the weight memory (ECC, checksum plus reload from a
> clean copy, periodic scrubbing) work, measured as detection coverage, detection-to-recovery latency, logic and memory cost, and
> accuracy lost while a bad weight goes unnoticed, when the checker logic can fail too? And does software-level fault injection
> (the free MRFI tool) predict what the hardware actually does?

Method: inject the **same fault list** three ways: MRFI at the PyTorch level, the Verilog in simulation, and the DE2-115 board.
Report where the software estimate is wrong.

**Hypothesis, written down before any test (Skeptic):** MRFI agrees with the board, within confidence intervals, for unprotected
arrays and voted accumulators; MRFI **underestimates silent errors by at least 2x** when checker faults or corrupted shared
weights are present, because it does not model the checker or the reload path. Either outcome is reported.

**Why this beats the plain benchmark:** it asks one question that can have a wrong answer, builds on a real G19 finding
(voting does nothing for corrupted shared weights; only the checksum detects them, and only with a clean copy), reuses all G19
assets, and gives the authors of the free tools a reason to care.

## What was rejected, and why
- **A generic fault-injection framework:** already free (MRFI, ENFOR-SA, TensorFI, PyTorchFI). Rebuilding it wastes the time.
- **A per-layer protection selector (the Advocate's own idea, conceded):** layer-selective hardening and selective TMR are
  published, and the 64-32-10 network has only two layers to choose between.
- **The accumulator-side ranking as a headline:** triple voting beating duplicate-and-compare is textbook. Keep only the
  "checksum collapses at high rates" sentence as supporting evidence.
- **Everything already ruled out:** MoS2, YC/Starcloud wording, "chip", "beam-ready", calling register injection on an SRAM
  FPGA radiation testing, extrapolating the tiny network, the RV64 core.

## Honest novelty statement
At most: "a measured, open, small-FPGA comparison of weight-protection strategies, and a hardware check of a free software
tool." Do not claim more until the prior-art checks below are done. Related work the members found (verify before citing):
MRFI (arXiv 2306.11758), ENFOR-SA (arXiv 2602.00909), HarDNN (arXiv 2002.09786), cross-layer selective redundancy
(arXiv 2312.13754), SEU vulnerability of BNNs on SRAM FPGAs (arXiv 2404.01757), checksum (ABFT) protection of DNN accelerators
(arXiv 2402.10850).

**Prior-art checks still outstanding:** has anyone validated MRFI, PyTorchFI, TensorFI or ENFOR-SA against hardware or RTL
injection; published comparisons of RTL-level and software-level injection for DNNs; checker or voter faults in ABFT;
ECC versus checksum+reload versus scrubbing of DNN weight memory; whether the Cyclone IV's own CRC-based error detection
already covers part of this.

## Confidence that it is fundable within 6 months (small fellowship, placement or sponsored equipment)
| Member | Confidence | What lifts it |
|---|---|---|
| Advocate | 55 | a sponsor found, board result by week 8 |
| Skeptic | 40 | about 65 with a sponsor and bit-exact board inference before 30 Nov |
| Investor | 45 | about 65 once a faculty sponsor agrees |
| Pragmatist | 65 | the slice below published on time |

**Reading it:** roughly even odds for a modest outcome. The realistic ceiling is a fellowship placement, a university or vendor
donation through a faculty sponsor, and a workshop paper. No member found a route to venture money, and none would pitch for it.

## Funding map (Investor; verified where marked)
1. **Science Academies Summer Research Fellowship 2027 (IASc, INSA, NASI).** About two months at an Indian research lab, with a
   stipend and train fare. The official portal (webjapps.ias.ac.in/fellowship2027) gives the application deadline as **30 November
   2026**, results around March to April 2027, and requires a teacher's email (the Academy contacts them for a recommendation).
   **Not confirmed from the official page:** the year-of-study rule and a 65% marks rule (aggregators only). Check inside the form.
2. **A faculty sponsor at the student's own university, then vendor donations.** AMD's university donation program takes requests
   only from faculty. Altera University has a limited donation program. A sponsor turns closed channels into open ones.
3. **MeitY Chips to Startup / ChipIN (C-DAC):** shared tools and boards for participating institutions. Whether the student's
   university takes part is unconfirmed; ask the department head.
4. **ISRO's student satellite route:** a long shot; the realistic version is joining a campus CubeSat team.
5. **NLnet (open hardware):** the last call closed June 2026; the successor is unverified.
What a funder pays for: a person with a sponsor and a hardware-validated, open result, not a concept.

## The plan
**Before 30 November (about 3 weeks), simulation only, labelled as such:**
1. Add SECDED ECC and periodic scrubbing to the existing weight-upset simulation (the G19 campaign).
2. Port the network to MRFI and compare it with the Verilog run on the same fault list; accuracy must agree within statistical error.
3. Write the hypothesis above and date it; commit it before running the comparison.
4. A two-page PDF plus the public repository: the bit-exact G19 results, the five-scheme plot, the failed checksum hypothesis,
   and one paragraph proposing the cross-check. Ask a faculty member (VLSI, embedded or FPGA; fault tolerance is ideal) to recommend
   and to sponsor; submit the application by about 25 November.
5. The statement may claim: a simulated, bit-exact, open study and a narrow testable question. It must not claim radiation
   testing, "beam-ready", spacecraft-grade, novelty, MoS2 or YC.

**The first email to a faculty member needs:** the question in one line, the PDF and repository, a small specific ask (a
recommendation or a credited project, no money), how much of their time it takes (for example 30 minutes every two weeks), and
an offer that the result could become a co-authored workshop paper.

**Weeks 1-12 (the first three are the slice above):**
- **Week 3:** slice published; MRFI and Verilog agree on accuracy. If not, fix the fault model first.
- **Week 6:** FSM sequencer plus UART or JTAG-UART readout bit-exact on the board. If not, stop the hardware work and keep the
  simulation paper.
- **Week 9:** at least three schemes measured on the board, plus faulty-checker runs, with logic elements, memory blocks, Fmax and
  retry latency per scheme from the Quartus reports. Otherwise report only the schemes measured.
- **Week 12:** open release that reproduces everything with one script, an arXiv or workshop note that states every limit, an
  upstream issue or contribution to MRFI, and about 10 emails to radiation-effects and CubeSat groups.
- **Stop outreach and keep it as a portfolio piece** if there is no substantive reply by week 12.

**Fit on one DE2-115 (Pragmatist, from vendor listings; confirm in the manual):** 114,480 logic elements and about 3.9 Mbit of
embedded memory. The 2,368 weights (about 19 Kbit) with a golden copy and ECC bits stay under 50 Kbit; the 2,700 test images
(about 1.4 Mbit) also fit on-chip. The on-board USB-Blaster allows a JTAG UART with no extra hardware.

## Traps
Calling register injection "radiation testing"; "beam-ready"; the word "chip"; extrapolating a 2,368-weight network to real
models; reviving MoS2 or the RV64 core; quoting the YC request or Starcloud; claiming novelty before the checks above.
