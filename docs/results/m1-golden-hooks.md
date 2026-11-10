<!-- Split from docs/results.md on 2026-11-10 without changes to the text; the index of all result sections is docs/results.md. -->

## M1: hooks in the golden emulator (riscv-emu, branch `radslice-hooks`)

The golden core is unchanged in behaviour. Two optional backends, both null by default, were
added: `RegFileBackend` (replacing the integer register file behind `read_reg`/`write_reg`) and
`AluBackend` (one entry point, `exec(op, a, b)`, covering the register-register and immediate
ALU forms, the comparisons behind branches, and data-address generation for loads, stores and
JALR). The PC adders, M/A/F/D and CSR operations are not routed through it.

- The brief expected 14 direct `regs[]` uses; there were 3 (two inside the accessors themselves
  and one in `dump_registers`). The real work was the ALU, which was inlined in about 25 places.
- Verification: `ctest` 16/16 before and 17/17 after (one new suite checking that every ALU form
  reaches the backend with the right operation and operands, and that a backend can steer
  results); the official riscv-tests 125/125 before and 125/125 after (3 rv64si tests do not
  assemble with this toolchain, and 2 rv64mi tests are excluded by the existing runner; all
  unchanged).
- Performance: on a fixed 300M-instruction loop, 12 alternating runs, the modified build took a
  median 0.976x the baseline's time. The run-to-run spread on this laptop was 2.2x, so this means
  no slowdown was detected, not that none exists below a few percent.
- Found, not fixed: `riscv_emu --dump` segfaults in the unmodified `main` build here.
