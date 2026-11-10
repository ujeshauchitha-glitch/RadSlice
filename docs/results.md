# Results

Results come from scripts in this repository with fixed seeds. Every number is
a simulation under tier-C assumptions, not a measurement. Rejected ideas and
failures are recorded here, not deleted.

This file was one 1,365-line document until 2026-11-10; it is now split by study, one file per
section, with the text unchanged (`docs/results/`). Read `summary.md` first: it has the
conclusions C1 to C17, each with what it rests on and what would overturn it, and the collected
list of rejected ideas and corrections. The study sections below hold the evidence.

| File | Section | Lines |
|---|---|---|
| [`results/summary.md`](results/summary.md) | M5: what was shown, what was not, and what would change it (conclusions C1 to C17) | 136 |
| [`results/p-check.md`](results/p-check.md) | Checking p against the primary source (Ao, Zhou et al. 2025) | 139 |
| [`results/g09-clustered-defects.md`](results/g09-clustered-defects.md) | G9: clustered defects | 75 |
| [`results/g10-crossbar-cost.md`](results/g10-crossbar-cost.md) | G10: the spare-routing crossbar cost and the K trade-off | 78 |
| [`results/g14-routing-defects.md`](results/g14-routing-defects.md) | G14: the routing's own defects | 27 |
| [`results/g15-g16-fault-classes-seeds-k.md`](results/g15-g16-fault-classes-seeds-k.md) | G15 and G16: more fault classes; seeds and K | 38 |
| [`results/g11-g13-literature.md`](results/g11-g13-literature.md) | G11, G12, G13: logic style, process mechanisms, space barriers (literature) | 71 |
| [`results/g17-vliw.md`](results/g17-vliw.md) | G17: four parallel units that are also the spares (VLIW) | 40 |
| [`results/g18-fpga-core.md`](results/g18-fpga-core.md) | G18: the slice-serial core in Verilog and the DE2-115 (steps 1 to 5, K follow-up) | 137 |
| [`results/g20-weight-protection.md`](results/g20-weight-protection.md) | G20: weight-memory protection (preregistered; 3 of 6 predictions held) | 30 |
| [`results/g19-mac-array.md`](results/g19-mac-array.md) | G19: the int8 MAC array (steps 2 to 4: lockstep, self-test and remap, upsets and protection) | 100 |
| [`results/m0-closed-form-yield.md`](results/m0-closed-form-yield.md) | M0: closed-form yield model | 29 |
| [`results/m1-golden-hooks.md`](results/m1-golden-hooks.md) | M1: hooks in the golden emulator | 20 |
| [`results/m2-sliced-datapath.md`](results/m2-sliced-datapath.md) | M2: sliced ALU and register file, zero faults | 83 |
| [`results/m3-defects-selftest.md`](results/m3-defects-selftest.md) | M3: permanent defects, boot self-test and remap | 136 |
| [`results/m4-transient-faults.md`](results/m4-transient-faults.md) | M4: transient faults, protection schemes and fault-injection campaigns | 221 |

Reading order for the current direction (Track A, weight-memory protection):
`g19-mac-array.md`, then `g20-weight-protection.md`, then `summary.md`. For the 2D-materials study
(Track B): `summary.md`, `p-check.md`, `m0-closed-form-yield.md` to `m4-transient-faults.md`, then G9 to G18 in order.

The headline results that `make reproduce` regenerates are G19 step 4 (`results/g19_campaign.md`,
`results/g19_campaign.png`) and G20 (`results/g20_weight_protection.md`); the rest need the C++ build,
a RISC-V gcc or Icarus Verilog, as listed in `summary.md` under "Reproducing everything".
