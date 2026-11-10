<!-- Split from docs/results.md on 2026-11-10 without changes to the text; the index of all result sections is docs/results.md. -->

## G14: do the spares and routing survive their own defects? (done after G10)

`python scripts/routing_defect_study.py` (about 12 s, exact models) writes `results/routing_defect_study.{csv,md,svg}`;
the model is `python/radslice/routing_model.py` (10 tests; with q = 1 it reproduces G10). Each spare carries its
private routing (tag, compare, multiplexer inputs), which fails with it, so a spare's yield is p x q^(its private
transistors); at the G10 optimum that is 414 transistors per spare. A shared block of 1,152 transistors (tier C) is
fatal unless replicated. q is the per-transistor yield: 0.9992 (isolated device, tier A) or 0.99762 (implied by the
register yields, tier B).

| # | Claim | Verdict | Numbers |
|---|---|---|---|
| 1 | A spare's own routing lowers its yield and raises the spares needed | **Confirmed** | Spare yield at the G10 design: 0.710 (fault-free routing), 0.510 (q = 0.9992), 0.265 (q = 0.99762). Cheapest register file, spares' private routing only: 8 regions in every case, 192 spares (4.21x the bare file per good file) becomes 280 spares (5.72x, 1.36x) at q = 0.9992 and 568 spares (10.58x, 2.52x) at q = 0.99762. |
| 2 | The shared block is a single point of failure that needs replicating | **Confirmed, and the cost depends on an assumption I made up** | One copy of the 1,152-transistor block yields 0.064 at q = 0.99762 (the same as a 64-bit register, 7%). With ideal selection among copies (optimistic) the design needs 8 copies at q = 0.9992 (6.10x per good file, 1.44x) and 48 at q = 0.99762 (12.75x, 3.01x). The block's size is tier C. |
| 3 | The cheapest design changes | **Partly: the region count does not, the spare count and cost do** | 8 regions throughout; spares 192 to 288 to 576; per-position steering (the naive topology) is hopeless at q = 0.9985 and below (no design above 50% yield within 1,200 spares). |
| ALU | Spares of the ALU carry routing too | **Single unit survives; replicated does not** | A single reused unit needs 2 to 16 spare units at q = 0.9992 and costs 6.10 to 6.81x per good chip across K = 1 to 32. The replicated ALU needs 34 to 36 spares at K = 8 to 32, and at K = 1 has only 26% ALU yield even with 60 spares at q = 0.9992; at q = 0.99762 its chip cost is 57x to 265x. K* by silicon x cycles stays 32 for the single unit. |

What this changes. C13's "if any failure there were fatal, no spare design would yield" was too pessimistic
because routing mostly disables a spare, not the chip; and not pessimistic enough, because it left out that the
spares then need to be many more. The conclusion for the register file is a range, 1.4x to 2.5x on top of G10, with a
further 7% to 21% for the shared block, depending on a per-transistor yield nobody has measured for routing. Independent
defects only; clustering of routing defects is not modelled.

**Mistakes on the way.** The first search compared cost per good in two different units and found a design
with 0.6% yield "cheapest"; a debugging print of the q = 1 case, which must reproduce G10, showed it. The
search ranges were too small (a result at the cap of 800 spares or 8 copies is a limit, not a minimum) and were widened
until the optimum moved off the cap. The q = 1 rows differ from G10 by 3% only in the version that includes the shared block.
