# Council verdict: should RadSlice be pitched as a radiation-tolerant inference engine for spacecraft?

Method: four Claude instances with fixed roles (Advocate, Skeptic, Investor, Pragmatist) each gave an independent position, then
each saw the others' arguments and answered with concessions, holds, a 12-week plan, kill criteria and a confidence. Two rounds.
**Limit of the method:** the four share one model and one briefing, so their agreement is not independent evidence. Facts they
recalled from memory (listed at the end) are unchecked. Treat this as a structured stress test, not a verdict from the field.

## Verdict
The framing "radiation-tolerant inference engine for spacecraft" does not survive as the title, the claim or the ask. What the
evidence supports is an **open, reproducible on-board fault-injection benchmark for protected int8 inference**, with measured
hardware cost and coverage of five protection schemes under a stated fault model. Pursue that.

| Member | Confidence, framed as a spacecraft inference engine | Confidence, reframed plan |
|---|---|---|
| Advocate | 62 (already framed as a measurement platform) | 72 |
| Skeptic | 15 | 60 |
| Investor | 25 | 70 |
| Pragmatist | 30 | 75 |

## Agreed by all four
- "Spacecraft" and "inference" appear at most once, as a motivation sentence. Never in the title, abstract or the ask.
- Cut MoS2/2D material from any space pitch (no radiation data exists; 41 of 54 parameters are assumptions).
- Do not say "chip", "beam-ready", or quote YC or Starcloud until the official YC text has been read.
- No investor deck. The best outcomes, in order: a research position or internship, a workshop paper, a small grant, a startup last.
- Do not ask for a second board (it unblocks nothing; the Pragmatist keeps a cheap backup as a soft, off-critical-path idea).
- Do not ask for beam time in the next 3 months. Ask a faculty sponsor whose existing campaigns could carry the board later; a board
  that has never run gets a "no" that burns the contact.
- The protection comparison and the weight finding (voting does nothing for shared corrupted weights; only the checksum detects
  them, and only with a clean reload copy) are the publishable results, together with the honestly reported wrong hypothesis.

## What on-board fault injection can and cannot claim (Skeptic's conditions, accepted by all)
It validates: (1) the RTL matches the simulator under injection; (2) measured logic elements, memory blocks, Fmax and retry latency
per scheme; (3) detection coverage once faults are also injected into the voter and checksum logic (which the simulation treated
as fault-free). It does NOT validate real upset rates, configuration-memory upsets (the Cyclone IV keeps its configuration in SRAM),
latchup, functional interrupts or total dose. A hardware result is accepted only if it shows millions of inferences at the real
clock, the simulated curves reproduced within confidence intervals, and an explicitly stated fault model.

## Small disagreements, and how they are settled here
- **RV64 core or a small hardware sequencer to drive the array?** Pragmatist and Skeptic favour the sequencer (simpler, likelier to
  work); the core is mentioned in one line at most, or returns later. Settled: build the sequencer first.
- **When must the array be bit-exact on the board?** Weeks 3, 4, 4 and 5 were proposed. Settled: week 4.
- **How many people to contact?** 3 to 5 (Skeptic) or about 10. Settled: about 10, each with a specific question and a two-page summary.

## The 12-week plan
1. **Weeks 1-4:** a small FSM sequencer loads weights and images from on-chip memory, runs the 64-32-10 network and streams logits
   and counters to a PC over UART; a script diffs the output against the integer reference. Predictions for the next phase are
   committed to the repository before any board run.
2. **Weeks 4-8:** an LFSR drives the existing flip and stuck-bit hooks at programmed rates, into accumulators, weights, and the
   voter and checksum logic; all five schemes at real clock speed; compare with the simulated curves.
3. **Weeks 8-10:** report logic elements, memory blocks, Fmax and retry latency per scheme from the Quartus reports; tag an
   open-source release that reproduces everything with one script; write a short note (arXiv or a workshop) that keeps the failed
   checksum hypothesis and states every limit.
4. **Weeks 10-12:** email about 10 radiation-effects groups and CubeSat teams with the artifact and a specific question; ask for a
   sponsor or a position, not equipment.

## Kill criteria
- Not bit-exact on the board by **week 4**: stop the hardware track and publish the simulation alone as a short report.
- Board curves disagree with the simulation and the cause is not found within two weeks (by about week 8): publish the
  disagreement and stop extending the work.
- No substantive reply from about 10 contacts by **week 12**: stop pitching; keep it as a portfolio piece and paper.

## Unverified recollections in the debate (check before relying on any)
Phi-Sat-1 flew a Movidius Myriad 2; the HPE Spaceborne Computer uses commercial servers; Starcloud flew a commercial NVIDIA GPU;
Microchip RTG4/PolarFire and AMD space-grade parts exist; SELSE as a fitting workshop and its deadlines; Texas A&M and
Berkeley cyclotron programs and whether they take student experiments; SignalTap availability in Quartus Lite; whether a supported
way to emulate configuration-memory upsets exists on a Cyclone IV.
