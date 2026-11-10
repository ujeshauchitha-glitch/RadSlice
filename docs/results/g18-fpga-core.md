<!-- Split from docs/results.md on 2026-11-10 without changes to the text; the index of all result sections is docs/results.md. -->

## G18, step 1: the slice-serial ALU in Verilog matches the model

`rtl/slice_unit.v` (one K-bit slice unit) and `rtl/sliced_alu.v` (one unit reused once per beat, single-unit
organisation of G10). `bash scripts/run-rtl-tests.sh` (Icarus Verilog) feeds `rtl/tb/tb_sliced_alu.v` the vectors written
by `radslice_alu_vectors`: 15 operations over 23 edge values squared plus 20,000 random pairs, 27,935 vectors per K.
The model generating them is checked against the golden core's `alu_reference` first (no difference at K = 8, 16 or 32).

- **Result:** the hardware's result and its number of busy cycles equal the model's result and `beats_for` on every
  vector at K = 8, 16 and 32. The beat count is therefore the hardware's cycle count, not a separate assumption.
- **Does the test bite?** Three deliberate breaks (arithmetic shift fill removed, carry not propagated, left funnel
  shifted by one extra bit) made 1,392, 4,910 and 3,378 of 27,935 vectors fail at K = 16.
- **Size, a generic estimate:** Yosys with 4-input LUTs gives 1,320 (K = 8), 1,539 (K = 16) and 1,953 (K = 32) LUTs and
  205 flip-flops. The DE2-115 has about 114,000 logic elements, so the ALU alone is 1% to 2% of it. Quartus will
  report different numbers; these only say the unit is small.
- **Not done:** Verilator lint (the OSS CAD Suite's wrapper needs a Perl module this machine lacks), timing, the register
  file, the core, spares, or any board run. Only the ALU is verified against vectors; nothing about instruction
  fetch or the pipeline is.

## G18, step 2: the word-parallel register file matches the model, and is too big

`rtl/sliced_regfile.v`: 32 words x 8 byte slices plus SPARES spare slices, a remap table, one parity bit per word, a
stuck-bit defect and an upset-injection port per slice, and direct physical access for the self-test. Two read ports
return a whole 64-bit word at once. `radslice_rf_vectors` writes 60,000 random operations (writes, reads, defects,
remaps to spares, bit flips, physical reads and writes); the hardware's value and parity flag equal the model's on
all 22,690 checked reads at 0, 8 and 32 spares. Four deliberate breaks (parity not written, flip shifted one bit,
defects ignored, remap off by one) failed 8,907, 4,005, 16,952 and 21,712 of 60,000 operations.

- **A failure on the way:** the first version produced about 48 wrong reads in 60,000 and passed no test. The cause was
  simulator sensitivity (Icarus does not re-evaluate a function that reads array elements when one changes), not a
  logic error; rewriting the read path as continuous assignments over packed vectors fixed it. A reminder that a test
  that fails rarely is still a failure.
- **It does not fit comfortably.** Yosys (generic 4-input LUTs, 8 spares) gives about 96,000 LUTs and 5,800 flip-flops,
  84% of the DE2-115's roughly 114,000 logic elements, before any core. Reading a whole word through the remap means
  eight 264:1 byte selectors per port. This is the cost of the word-parallel interface, not of the architecture:
  a slice-serial design reads one byte per beat, so one selector per port. The next step builds that version
  (`sliced_regfile_serial.v`) and the core uses it; this module stays as the whole-word reference.
- **Not done:** timing, the serial register file, the core.

## G18, step 3: the slice-serial register file

`rtl/sliced_regfile_serial.v` has the same slices, remap, parity and fault hooks as step 2 but moves one byte per beat
(one selector per read port; parity accumulated across the eight beats and compared on the last). The same 60,000
operation sequences, replayed one byte at a time, match the model on all 22,690 checked reads at 0, 8 and 32 spares.

- **Size, generic Yosys estimate (8 spares):** 38,700 LUTs, down from about 96,000 for the word-parallel file but still about
  a third of the DE2-115. Applying the defect after the byte is chosen (once per port, not once per slice) took off
  6,500 of those. The flip-flop form (about 5,800 flip-flops with their write multiplexers and 264:1 selectors) is
  what is expensive; an FPGA would hold the slices and the remap table in block RAM, which the serial access
  pattern permits, but that version is not built and its size is not measured. Treat the fit as **unresolved**, not
  as shown.
- **Not done:** block-RAM mapping, timing, any Quartus run.

## G18, step 4: the in-order RV64I core matches the golden emulator instruction by instruction

`rtl/rv64i_core.v`: an in-order, multi-cycle core made of the verified parts (one K-bit slice unit reused per beat, and the
byte-per-beat register file with remap and parity). It runs RV64I (32-bit encodings; no CSRs, M, A, F, D or C) and uses the
ALU where the golden core does (arithmetic and logic, branch comparisons, data addresses, the JALR target); LUI, AUIPC, JAL and
the link value use a plain adder. `radslice_trace` writes, from the golden core, one line per retired instruction (pc,
instruction word, register written and its value, store address/size/data, next pc); `rtl/tb/tb_rv64i_core.v` compares the
core's retirement stream with it line by line. `bash scripts/run-core-lockstep.sh` reproduces the table.

| workload | K | instructions compared | cycles | cycles per instruction | result |
|---|---|---|---|---|---|
| chacha20 | 8 | 47,993 | 1,249,100 | 26.02 | all match |
| chacha20 | 16 | 47,993 | 1,120,734 | 23.35 | all match |
| chacha20 | 32 | 47,993 | 1,056,563 | 22.01 | all match |
| coremark_like | 8 | 43,376 | 1,148,437 | 26.47 | all match |
| coremark_like | 16 | 43,376 | 1,009,219 | 23.26 | all match |
| coremark_like | 32 | 43,376 | 939,598 | 21.66 | all match |

- **The hardware's cycles are the model's beats plus a fixed cost.** Subtracting the model's ALU beats from the measured
  cycles leaves 992,368 (chacha20) and 870,001 (coremark_like) at both K = 8 and K = 16, to the cycle. At K = 32 the
  remainder is 12 cycles lower in both workloads; I have not found why (0.001% of the total), and it is recorded as
  unexplained.
- **The fixed cost is about 20.7 cycles per instruction, not the 1 the model charged.** It is one fetch, eight beats to read
  the register file, eight to write back, and a few control cycles, with no overlap between them. So in this
  implementation the ALU beats are only about 11% of the cycles at K = 16 (6% at K = 32, 21% at K = 8), and going from K = 8
  to K = 32 saves only 15% (chacha20) to 18% (coremark_like) of the cycles. The M2 trade-off between silicon and cycles was computed with a base of 1
  and is therefore too favourable to small K for this microarchitecture. A design that overlaps the register file with the ALU
  would change this; this core does not.
- **G17 survives the larger base:** rerunning the scheduler with a base of 20 gives a four-unit speedup of 1.65 to 1.83 on the
  two workloads (1.56 to 1.74 with base 1), so the "about 1.7x, not 4x" finding does not depend on that parameter.
- **Failures on the way:** the first testbench never finished compiling (a combinational block reading a 2 MiB array makes the
  simulator build an entry per element); a stale-wire read in the comparator made the first instruction look wrong; a 4-bit
  register field truncated register numbers above 15; and the first script run exited silently because the OSS CAD
  Suite's runtime libraries shadowed the ones the trace tool needs. None was a core bug; the core passed once the
  testbench was right.
- **Not done:** instruction-mix coverage beyond two programs (no rv64ui riscv-tests: they need CSRs and trap handling); faults
  in the lockstep; M, A, F, D, C and CSRs; synthesis of the whole core, timing, and any board run.

## G18, step 5: synchronous memory and the DE2-115 wrapper (simulation only)

- **Block-RAM timing.** The FPGA's block RAM returns data one clock after the address, so the core has a `MEM_SYNC`
  option that waits one extra cycle per instruction fetch and per load. With it, the core still matches the golden
  trace on every instruction: chacha20 (47,993 instructions, 24.42 cycles per instruction at K = 16) and
  coremark_like (43,376, 24.32). Without it nothing changed: the 300-instruction check gives the same 6,824 cycles as before.
  A first chacha20 run reported a failure that was a race in my testbench (it announced the final match one
  retirement late and the program's idle loop had already run past the end of the trace); fixing the testbench
  removed it and no core change was involved.
- **Board wrapper.** `rtl/de2_115/de2_115_top.v` puts the core on a divided clock (6.25 MHz) with on-chip ROM and RAM,
  memory-mapped LEDs, switches and displays, and the register file's flip and stuck-bit hooks on the keys and switches.
  `de2_115.qsf` (78 pin assignments copied from a published DE2-115 pin table and checked against the extracted text by
  script), `de2_115.sdc` and `de2_115.qpf` make it a Quartus project; `docs/fpga-board.md` says how to use it.
- **Simulated session** (`bash scripts/run-board-sim.sh`, whole wrapper plus core): the counter counts; the red LEDs follow the
  switches; flipping bit 12 of x6 moved the displayed counter from 72 to 4,207 and raised the parity-error count;
  a stuck bit on x6's first slice raised it to 78. A flipped bit is detected by parity and then spreads: the next write of
  the register stores the corrupted value with a fresh, matching parity, so the count rises by a few, not continuously.
- **Quartus compile (24.1 Lite, device EP4CE115F29C7), run on 2026-11-02, built from the pushed sources:**
  first attempt 102,196 of 114,480 logic elements (89%), 69,947 registers, 65,536 memory bits. That was almost all my
  data RAM: it was initialised from the ROM image with a non-constant expression, so the tool built it from
  flip-flops instead of block RAM. Rewritten as eight byte-lane arrays it maps to block RAM, and the same design then
  uses **17,757 logic elements (16%), 4,347 registers and 131,072 memory bits (3%)** with 8 spare slices.
  The register file with its remap, parity and fault hooks therefore fits easily, which the generic Yosys estimate (38,700
  LUTs) had left open; that estimate was about twice the real figure.
  Timing at the constrained core clock of 6.25 MHz: setup slack +15.6 ns, hold slack +0.15 ns at the worst corner;
  Fmax of the core clock domain 22.3 MHz (85 C slow model) and 24.1 MHz (0 C), so the clock could be raised to about 12.5 MHz
  (`CLK_DIV = 2`) with margin. 17 compile warnings, none about timing; the ones that matter are the I/O voltage reminder
  (23 pins on the jumper-selected banks) and that the ROM image has only 14 of its 2,048 words.
- **Still not done:** no board run (the programming cable was not detected by the laptop when checked), no check of the pin
  table against Terasic's manual, the boot self-test and remap are not wired to the board, and the data RAM starts zeroed
  (a program with initialised data would need per-lane image files).

## G18 follow-up: the K trade-off at the measured base

`scripts/m2_cycles.py` now also tabulates base = 21 (the Verilog core measured about 20.7 cycles per instruction outside the ALU: fetch,
eight beats to read and eight to write back the byte-sliced register file, and control), and `scripts/crossbar_study.py` repeats
E3 at base 2 and at base 21, and adds base 21 to the E4 sensitivity. Tables: `results/crossbar_study.md`.

- **The best K moves from 32 to 16.** Silicon x cycles for the compute loop, replicated ALU: K = 16 is 1.00, K = 32 is 1.00 to 1.01,
  K = 8 is 1.12, K = 64 is 1.28 (at base 2: K = 32 is 1.00, K = 16 is 1.37, K = 8 is 2.20). Single reused unit: K = 16 1.00, K = 32 1.01 to 1.02, K = 8 1.11 to 1.12.
- **The choice now matters little between 16 and 32** (within about 2%), and K = 8 costs about 11% more. That agrees with the cycle
  measurement: the ALU is a small part of each instruction's cycles when the register file is read and written a byte at a time.
- **ALU share of chip silicon at the K* design (single unit): 2.7%**, down from 9.1%; the register file and everything around it
  is what the chip is made of.
- **Caveat:** the measured base belongs to this core, which does not overlap register access with the ALU. A core that did would
  have a smaller base and move the answer back toward larger K. The conclusion is conditional on this microarchitecture.
