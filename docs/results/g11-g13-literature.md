<!-- Split from docs/results.md on 2026-11-10 without changes to the text; the index of all result sections is docs/results.md. -->

## G11, G12, G13: logic style, process mechanisms and the space barriers (literature, done after G15)

**How this section was made.** G11 is answered from the paper itself (the PDF, main text and Methods) and a
computation. G12 and G13 are a literature survey done by web search; for most sources only the abstract or
a search excerpt was read, which is stated per row. No number from these sources is used as a model
parameter; they tell us which fault types exist and what scale the claims have. Where a search found
nothing, the table says so.

### G11: the paper's logic is n-type only, so every earlier cost already assumed that style
- **Claim 1 (what the paper's library is): confirmed from the paper.** "Our RV32-WUJI is based on the MoS2
  n-type metal-oxide-semiconductor architecture" and "the entire RV32-WUJI chip uses only n-type MoS2
  transistors" (main text, pp. 656-659). The basic gate is an enhancement-and-depletion-mode ratioed
  inverter: one driver and one load transistor; gates use only the driver network plus a single load
  (p. 657). It runs at 4 V, with a longest-path delay of 171 microseconds (a few kHz). The authors say pass
  transistor logic "may be a compromise" and that 2D CMOS is the long-term aim. So the 144, 27 and 27
  transistor counts behind G10 and G14 are n-only ratioed counts.
- **Claims 2 and 3: computed** (`scripts/logic_style_study.py`, `results/logic_style_study.md`). A multiplier
  mu on every transistor count (a complementary n-input gate is 2n transistors, an n-only ratioed gate is
  n + 1: ratio 4/3 for two inputs, 8/5 for four, towards 2) makes a block yield p^mu. At mu = 4/3, 8/5, 2
  the slice yield is 0.633, 0.578, 0.504. With fault-free routing the cheapest register file stays at **8
  regions** but needs more spares (192, 264, 320, 424), and the cost per good file rises from 4.21 to 5.29,
  6.24, 7.83 times the file's own bare silicon, which is 7.1x, 10.0x and 15.7x the n-only bare file in
  absolute silicon. With the isolated-device routing yield (0.9992) the figures are 5.72, 8.06, 10.41,
  14.95 (10.8x, 16.7x, 29.9x absolute); at the register-implied routing yield and mu = 2 no register file reaches even 1e-6 yield. The K
  ordering does not move: K = 32 minimises silicon x cycles in every row, with the single reused ALU unit
  always cheaper than the replicated ALU (and the replicated ALU out of its search range at mu = 2).
- **What this does and does not say.** A complementary design is not "worse by mu": it is mu times the
  transistors for the same function, at an unchanged per-transistor yield, which is the pessimistic case.
  p-type devices may have a different per-transistor yield (a further factor not modelled); a complementary
  gate has no static current while a ratioed gate does (not modelled); and the paper's note that threshold
  control of load and driver sets the inverter's switching point means that threshold drift can break a
  ratioed gate's noise margin, not only flip a stored bit (see G13, not modelled).

### G12 and G13: literature table
Rows say what a source reports, what it predicts for a fault model, and whether RadSlice models it. "Read"
says how much of the source was read.

| # | Topic | What the source reports | Read | Predicts | Modelled here? |
|---|---|---|---|---|---|
| 1 | Threshold-voltage spread, cause | In exfoliated devices a large device-to-device spread of threshold voltage is attributed to unclean interfaces, trapped substrate charge and sulfur vacancies; oxygen plasma moved the mean from -18.7 V to -0.9 V and reduced the spread ([arXiv 1607.07500](https://arxiv.org/pdf/1607.07500)). Adding or removing sulfur vacancies shifts threshold voltage in opposite directions ([Nanoscale 2015](https://www.doi.org/10.1039/C5NR00253B)). | abstracts | A gate whose load and driver thresholds drift out of match fails at the logic level (wrong output level), not necessarily as a stuck output bit | No: all permanent defects are stuck bits |
| 2 | Variability at wafer scale | Synthetic monolayer CVD MoS2: threshold variation consistent with charge density and trap variations as low as about 1e11 cm^-2 ([Smithe et al., Nano Lett. 2017](https://poplab.stanford.edu/pdfs/Smithe-LowVariabilityMoS2-nano17.pdf)); a 1300-device study says the effect of scaling on variability is not yet systematically known ([Nano-Micro Lett. review](https://www.ncbi.nlm.nih.gov/pmc/articles/PMC9849648/)) | abstracts | Variability can be low when the process is clean; the paper's 71% suggests the process behind it is not at that level | Partly: p is taken from the paper's measurement |
| 3 | Grain boundaries | Grain boundaries modify transport and the effect depends on boundary type and current direction (review excerpt). No source found that ties grain-boundary density to yield | search excerpt | Clustered failures if boundaries are unevenly spread, but no quantified link | G9 models clustering generically; no process link |
| 4 | Contacts | Contact resistance limits scaling; current crowding at the metal-TMD interface raises it further; doping that tunes threshold can leave devices hard to turn off ([Nano-Micro Lett. review](https://www.ncbi.nlm.nih.gov/pmc/articles/PMC9849648/), arXiv excerpt) | search excerpt | Slow or weakly driven gates: timing failures, not stuck bits | No: no timing faults |
| 5 | Encapsulation | Unencapsulated MoS2 FETs lose on-current and shift threshold with moisture and oxygen; a spin-coated HMDS layer kept mobility within 10% over 25 days ([arXiv 2403.05885](https://arxiv.org/pdf/2403.05885)); in one comparison an Al2O3 cap lowered yield by 20% against other caps (search excerpt, source not opened) | abstracts | Packaging and encapsulation are a yield input and a lifetime input, and the choice that protects may cost yield | No: sweepable as a multiplier on p, not done |
| 6 | The paper's own process | Threshold control "at wafer scale is crucial for achieving a high overall yield"; yield was improved by co-optimising the process with a machine-learning method (main text p. 656) | full text | Threshold control is the lever the authors name | See rows 1 and 2 |
| 7 | Total ionizing dose | Few-layer MoS2 FETs with polymer-electrolyte gates reported up to 3.75 Mrad(Si), recoverable by annealing at 100 C, against about 1 Mrad(Si) for earlier silicon and 2D rad-hard parts, by the same group ([arXiv 2110.02601](https://arxiv.org/pdf/2110.02601)) | abstract | A single-group claim on single transistors | No dose model |
| 8 | Gate dielectric under radiation | ZrO2 and h-BN gates show opposite threshold shifts under irradiation: the channel can survive while the gate stack drifts ([Wang et al., TNS 2019](https://resource.aminer.org/pub/5ce2d0b1ced107d4c63a8510/total-ionizing-dose-response-of-mos-transistors-with-zro-and-h-bn)) | abstract | Dose moves thresholds, which matters most for ratioed logic | No |
| 9 | Displacement damage | Monolayer FETs tolerated 1.56e13 MeV/g under 30 keV protons; the authors note few studies address displacement damage ([J. Semicond. 2024](https://www.jos.ac.cn/article/doi/10.1088/1674-4926/24090027)) | abstract | Permanent defects arrive with fluence | Yes, as the in-field defect class (G15), rate tier C |
| 10 | Heavy ions | An exfoliated single-layer MoS2 FET was destroyed at 4e11 ions/cm^2 of 1.14 GeV uranium while a graphene device survived ([arXiv 1304.3614](https://arxiv.org/pdf/1304.3614)) | abstract | Single-device destruction is possible at high LET and fluence | In-field defect class, rate tier C |
| 11 | Single-event upsets in 2D circuits | None found for MoS2 circuits or memory. The closest hit cites laser-induced single-event transients in black-phosphorus MOSFETs | search | Unknown | Upset rates are tier C because of this |
| 12 | Atomic oxygen, UV | In lubricant studies atomic oxygen oxidises the MoS2 surface with sulfur loss, confined to the top layer; sputtered films flown 43.5 h in low Earth orbit were oxidised ([Tribology Lett.](https://dx.doi.org/10.1007/s11249-004-8094-9) and a Shenzhou-7 exposure study, search excerpts). For a one-to-few-layer channel the top layer is the channel | excerpts | LEO needs encapsulation; deep space does not have atomic oxygen | No |
| 13 | Vacuum, thermal cycling | Nothing found for MoS2 electronics | search | Unknown | No |
| 14 | The lubricant record | Lubricity is a bulk or sputtered-film property, and the studies above are about friction and wear | excerpts | Says little about a transistor channel | n/a |
| 15 | p-type devices | MoS2 pins near the conduction band (n-type); WSe2 pins near mid-gap and is ambipolar, which spoils off-state behaviour; hole-injection contacts (MoOx, W1-xCrxSe2 alloy) and a hetero-channel WSe2 p / MoS2 n inverter with gain above 10 and about 7 pW static power are reported (search excerpts: [ACS AMI 2025](https://par.nsf.gov/biblio/10399172), [PMC 7047721](https://pmc.ncbi.nlm.nih.gov/articles/PMC7047721/), a dissertation) | excerpts | Complementary 2D logic exists as demonstrations, not as a manufacturable process | G11 with a multiplier |

### Verdicts
- **G11 claim 1 confirmed; claims 2 and 3 computed.** The conclusions about the register file (8 regions) and
  K are unchanged by the style; the numbers move a lot (above).
- **G12 claim 1 (mechanisms): answered with the table above.** The sources support sulfur vacancies and
  interface charge as the cause of threshold spread, and nothing in the survey ties a mechanism to
  clustering at a particular scale. **Claim 2 (is a stuck bit the right fault?): not supported.** Threshold
  and contact mechanisms predict logic-level and timing faults that the boot self-test as written would
  not find; this is a limit of the fault model, stated, not fixed. **Claim 3: yes**, encapsulation is named
  as a yield input, with a reported 20% yield effect in one comparison; not used as a parameter.
- **G13 claim 1 (separating what RadSlice can and cannot represent): done in the last column.** The ones it
  can represent are the in-field defect arrival (rows 9 and 10) and upset rates; the ones it cannot are
  threshold drift, dielectric charging, timing faults, atomic oxygen and p-type devices.
- **Limits of this survey.** Every row except row 6 (the paper itself) rests on an abstract or a search excerpt, not on a full reading;
  the radiation numbers come from three research groups; and absence of results for single-event effects
  or vacuum behaviour means "not found by these searches", not "does not exist".
