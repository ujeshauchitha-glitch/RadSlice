<!-- Split from docs/results.md on 2026-11-10 without changes to the text; the index of all result sections is docs/results.md. -->

## G10: what the spare-routing crossbar costs (done after G9)

`python scripts/crossbar_study.py` (about 20 s, exact models, no simulation) writes
`results/crossbar_study.{csv,md,svg}`. It costs the routing that lets a spare stand in for a bad slice, in
transistors, with every published anchor from the Nature paper (144 T per 8-bit register block, 27 T per
1-bit adder with control, 27 T per four-input selection module) and every other choice a tier-C knob
(`config/params.toml`, `[cost.*]`; `python/radslice/cost_model.py`, 17 tests). p = 0.71 throughout.
Transistor count stands in for area; wiring and layout are not counted, so every cost below is more likely
too low than too high.

**What is costed.** Two ways to steer a spare into the register file. *Tagged*: spares sit in a side array,
each holding a tag naming the slice it replaces, every access is compared against all tags, and the output
lane multiplexers gain one input per spare that reaches the lane. *Per-position*: every one of the 256 slice
positions has its own multiplexer over the spares in reach, the naive crossbar; an upper bound. Spare *reach*
is the number of regions (1 = the ideal pool). For the ALU, two organisations, because M2 and M3 modelled
one of them and I had not said so: *replicated* (one physical unit per slice position, 64/K of them, plus
spares) and *single* (one K-bit unit reused for all 64/K positions, plus spares; this is what the fabricated
chip does at K = 1). The beat model is the same for both, so in this model the replicated organisation
costs more silicon for no gain in cycles.

### Verdicts on the four claims

| # | Claim | Verdict | Numbers |
|---|---|---|---|
| 1 | The ideal crossbar costs more than the slices it protects | **Confirmed, by a large factor** | Tagged routing is about 10.8x the storage of the spares it serves: 128 spares in one pool add 0.50 of a bare register file in storage and 5.41 in routing. Per-position routing for the same pool is 128x the bare file. |
| 2 | The cheapest design is not 128 spares in an ideal pool | **Confirmed** | Independent defects, tagged routing: the cheapest silicon per working file is **8 regions, 192 spares** (yield 0.929, silicon 3.91x the bare file, **4.21** per good file), against **7.11** for the 128-spare ideal pool and 1.54 if the crossbar were free. The crossbar multiplies the cost per good file by 2.7x at the optimum and 4.6x at the ideal pool. The optimum is broad: 4 regions 4.50, 16 regions 4.83. With per-position steering it moves to 32 regions (14.18). Reaching 99% yield: 8 regions, 224 spares, 4.43. With die-level clustering (alpha = 9.4): 8 regions, 224 spares, 5.10 against 9.44 for the pool; 99% needs 352 spares, 6.39. No sparing yields 8e-39. |
| 3 | The ordering of K changes once routing is costed | **Partly: the ordering by yield does not matter much; speed decides** | The register file is about 90% of chip silicon, so ALU width and organisation move the chip's silicon little: a single unit goes from 4.21 (K = 1) to 4.74 (K = 32), +13%; the replicated ALU is cheapest at K = 8 (4.86) and 5.29 to 5.40 at K = 1 and 32. Silicon times cycles is then decided by cycles: the widest sliced K (32) wins, K = 1 costs 14x more (alu_mix, base 2). K = 64, the unsliced ALU, needs 29 spare 64-bit units and costs 7.13 (+50% over the single unit at K = 32), so full width loses on silicon. |
| 4 | Conclusions depend on which tier-C assumption | **Mostly robust, with named exceptions (below)** | |

### Where the answers move (E4: one knob at a time)
- The register-file optimum stays at **8 regions, 192 to 200 spares** under every knob except the
  per-position topology (32 regions, 320 spares). Cost per good file ranges 3.30 (free tag compare) to 6.63
  (4x multiplexer cost) for the tagged design, 14.18 for per-position; never below 3.3, i.e. at least 2x the
  1.54 that a free crossbar would predict.
- K* by silicon x cycles is 32 for most settings. It becomes **64 for the replicated ALU** when multiplexers
  cost 2x or more, with the tag compare at 27 T, with 60,000 T of other silicon, with base = 1, or with
  per-position steering: the 32-to-64 difference is only 3% to 5% at the default (1.03 to 1.05 against 1.00), so "32 or 64" is the honest answer. It
  becomes **16 for the single-unit ALU** only if an ALU unit is 16x an adder (4x does not move it).
- The share of chip silicon in the ALU is 4% to 18% at the single-unit K*.

### E5: the routing is made of transistors, and this study still treats it as fault-free
The remap logic, tag table and multiplexers are transistors with the same defect rate as any other. With the
per-transistor yield implied by the register measurements (0.99762, tier B), a block of just 291 transistors
that must be perfect already fails half the time; with the isolated-device figure (0.9992, tier A), 866. The
cheapest register-file design above has 79,488 routing transistors (the ideal pool 199,296). If any single
failure there were fatal the chance they all work would be 5.5e-83 (2.4e-28 at the isolated-device rate).
The single-unit ALU's routing, 324 transistors at K = 8 with 4 spares, would survive 46% of the time.
So the cost-per-good numbers above are **lower bounds that assume routing that cannot fail**. A real design
needs defect tolerance in the routing itself (its own spares, or logic that degrades gracefully), and that is
not modelled; it is the largest open item from this study, and it applies to M3's remap logic as well.

### What this changes
- **C2** stays true as an in-model ordering (shared pool beats private spares) but 0.97 yield at 128 spares
  for 1.5x the slices is not a design: costed, the pool is 6.9x the bare file, and a limited-reach design is
  cheaper per good file.
- **C6**: the areas "2.1x / 4x / 12x ALU area at K = 8 / 16 / 32" are for the replicated organisation. For a
  single reused unit the joint chip optimum uses 4, 6 and 12 spare units at K = 8, 16, 32 (ALU yield 0.998,
  0.993, 0.978), which is 1,404, 3,942 and 14,796 transistors of ALU in all; at chip level the ALU is the
  smaller part of the cost.
- The M2 beat model gives the replicated organisation no speed advantage, so in this model it is dominated
  by the single unit. M2/M3 modelled the replicated one; that choice was an interpretation, not stated as one.

### Mistakes made on the way (kept)
- A first prototype optimised the ALU's spares on its own, minimising ALU silicon per good ALU. It picked no
  spares at K = 8 with 6.5% ALU yield, because a bad ALU also wastes the register file. The metric is silicon
  per good *chip*, optimising register file and ALU jointly; the prototype is not in the repository.
- A first register-file topology put a multiplexer at every slice position; that is the *per-position* upper
  bound above, 3x to 20x more expensive per good file than the tagged design that I should have started from.
- The search stops at 420 spares (360 under clustering); the 64-region rows hit that cap and are not minima.

### What would falsify this
- A routing layout whose cost is far below transistor-count-times-multiplexer (or a steering scheme cheaper
  than the tagged array): the optimum would move toward the ideal pool.
- Wire-dominated routing, which would raise every cost here and push the optimum toward finer regions.
- An ALU that is much larger than an adder (16x) with a single unit: K* moves to 16.
- Routing that cannot be made defect-tolerant at modest cost: no spare design would then work at this
  per-transistor yield, and the register file would need a different approach altogether.
