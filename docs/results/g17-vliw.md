<!-- Split from docs/results.md on 2026-11-10 without changes to the text; the index of all result sections is docs/results.md. -->

## G17: four parallel units that are also the spares

Full tables: `results/vliw_study.md` (`python scripts/vliw_study.py`, after building `radslice_vliw`). Part 1 is measured
on the real instruction traces (fault-free sliced ALU, list scheduling inside basic blocks, a branch/jump/system
instruction ends a block, no speculation). Part 2 is a model; its three inputs are tier C (`vliw.units`,
`vliw.port_area_frac` = 0.30, `vliw.rf_yield_one_unit` = 0.99).

**Measured.** Four units give about 1.7x, not 4x. chacha20 1.65x in order and 1.74x reordered at K = 16;
coremark_like 1.56x and 1.71x; the synthetic alu_mix 1.31x and 1.70x. The speedup hardly depends on K (the
dependences are the same; only the unit latency changes). The first extra unit gives most of the gain; going from three to
four units adds between 0 and 13 percentage points, depending on the workload and the issue mode. A single-unit in-order schedule reproduces `instret x base + beats` exactly
and the beat counts match `radslice_run`, which is the check that the tool measures the same machine.

**Modelled (K = 16, reordered issue, port area 0.30 per extra unit).**

| design | throughput per fabricated chip | chip yield | merit (throughput per silicon, vs one unit) |
|---|---|---|---|
| 1 unit, no spare | 0.50 | 0.50 | 1.00 |
| 1 active + 3 spares | 0.93 | 0.93 | 1.80 |
| 4 active, graceful (dead unit costs width) | 1.36 | 0.92 | 1.41 |
| 4 active, all must work | 0.11 | 0.06 | 0.11 |

- Four units that must all work are the wrong design: the chip yield is 6% at K = 16 and under 1% at K = 32.
- Letting a dead unit cost width instead of the chip (graceful) keeps the yield at 92% and gives the most speed per
  fabricated chip (1.36 against 0.93), so it is the design that meets the goal of speed.
- Per unit of silicon, one active unit with three spares beats the graceful design at every K tried (K = 16: 1.80
  against 1.41), because the register file is about 90% of the silicon and every extra unit adds ports to it.
- The graceful design stays ahead of one unit with no spare only while each extra unit adds less than about 0.41
  (K = 8), 0.56 (K = 16) or 0.76 (K = 32) of a register file in ports. Above that, extra units lose.

**Failures and limits recorded.** My first version of the trace tool ran past the end of each program (it did not
look for the test-finished write that `Cpu::run` looks for) and scheduled the idle spin loop; it reported no speedup
at all for chacha20 and coremark_like. The tests did not catch it; comparing the instruction count with
`radslice_run` did, and the corrected numbers above are from the fixed tool. The gap text (G17) assumed one load/store and one branch resource per bundle; the scheduler does not limit them (any
unit can take a load, store or branch), which flatters the wider designs by an amount not measured here. Not modelled: the front end that
fetches a wide word (a bundle's instruction fetch and decode), branch prediction, memory latency beyond one base
cycle, register renaming, multiplication and floating point (they end a block here, which understates the speedup),
and the routing between units and the register file as its own defect source. The unit yield is p^(K/8) with the
slice yield p = 0.71 (tier B); K = 32 units are therefore only 25% likely to be good, which drives the K = 32 rows.
