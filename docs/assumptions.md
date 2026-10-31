# Assumptions register

Generated from `config/params.toml` by `python -m radslice.assumptions --write`; do not edit by
hand (a test fails if this file drifts from the parameters).

Tiers: **A** published measurement (source given), **B** calibrated model (source given),
**C** hypothetical. A simulation that uses any tier-C value is a what-if, not a measurement.

**57 parameters: 8 tier A, 5 tier B, 44 tier C.**

13 parameters have a source: `cost.adder_bit_transistors`, `cost.mux2_transistors`, `cost.rf_block_transistors`, `cost.selection_module_transistors`, `cost.store_bit_transistors`, `yield.cluster_alpha_min_matrix_scale`, `yield.implied_per_transistor`, `yield.measured_64bit_register`, `yield.measured_8bit_shift_register`, `yield.measured_8bit_shift_register_from_map`, `yield.measured_inverter`, `yield.measured_transistor`, `yield.p_slice_good`. All come from one paper (Ao, Zhou et al., Nature 640, 654-661, 2025: main text and Supplementary Fig. 7) and are all about yield. Every other parameter, including every upset rate, is tier C, and every result in `docs/results.md` is still conditional on those.

| sourced parameter | source |
|---|---|
| `cost.adder_bit_transistors` | Ao, Zhou et al., Nature 640, 654-661 (2025), Extended Data Fig. 5 |
| `cost.mux2_transistors` | Derived: the 27-transistor four-input selection module is three 2-input selections |
| `cost.rf_block_transistors` | Ao, Zhou et al., Nature 640, 654-661 (2025), main text p. 659 |
| `cost.selection_module_transistors` | Ao, Zhou et al., Nature 640, 654-661 (2025), Extended Data Fig. 5 |
| `cost.store_bit_transistors` | Derived: 144 transistors per 8-bit register block, divided by 8 |
| `yield.cluster_alpha_min_matrix_scale` | Derived from the Supplementary Fig. 7 wafer map (28 matrices of 8 registers) by a one-sided likelihood-ratio test, python/radslice/cluster_model.py |
| `yield.implied_per_transistor` | Derived: 0.71 over 144 transistors; the 64-bit figure (~7% over 1,152) gives 0.99769 |
| `yield.measured_64bit_register` | Ao, Zhou et al., Nature 640, 654-661 (2025), main text p. 659 |
| `yield.measured_8bit_shift_register` | Ao, Zhou et al., Nature 640, 654-661 (2025), main text p. 659 and Supplementary Fig. 7(c) |
| `yield.measured_8bit_shift_register_from_map` | Supplementary Fig. 7(c), the wafer map, transcribed by scripts/si_fig7_extract.py into results/si_fig7_wafer_map.txt |
| `yield.measured_inverter` | Ao, Zhou et al., Nature 640, 654-661 (2025), main text p. 657 |
| `yield.measured_transistor` | Ao, Zhou et al., Nature 640, 654-661 (2025), main text p. 657 |
| `yield.p_slice_good` | Calibrated to the measured 8-bit shift-register yield above (Nature 640, 654-661, p. 659); the lower of the two readings is used |

| parameter | value | unit | tier | note |
|---|---|---|---|---|
| `alu.slice_width_bits` | 16 | bits | C | K. Swept over 8, 16, 32. Each ALU op takes 64/K beats. |
| `cost.adder_bit_transistors` | 27 | transistors | A | A 1-bit controlled adder with its control logic. An ALU that also shifts, compares and does logic is bigger than an adder; that is the tier-C factor cost.alu_factor. |
| `cost.alu_factor` | 1 | x adder | C | ALU unit silicon as a multiple of an adder of the same width. Swept 1, 4, 16. |
| `cost.alu_organisation` | replicated (one unit per position, as in M2/M3) and single (one K-bit unit reused) | model | C | Which one the fabricated chip's wording implies is ambiguous for K > 1; the paper's chip is K = 1 and reuses a single 1-bit ALU. Both are costed. |
| `cost.area_proxy` | transistor count | model | C | Silicon area is taken as proportional to transistor count. Wiring, vias and layout are not counted; a crossbar is wire-dominated, so this probably understates routing cost. |
| `cost.logic_style_multiplier` | 1, 4/3, 8/5, 2 | x transistors | C | Transistors per function relative to the paper's n-type-only ratioed library (1). A complementary n-input gate has 2n transistors and a ratioed n-only gate n + 1 (4/3 for 2 inputs, 8/5 for 4, towards 2). |
| `cost.mux2_transistors` | 9 | transistors | B | Cost of one 2-input multiplexer; an m-input multiplexer is taken as 9(m-1). A different circuit style would change it; the study varies it from 0.5x to 4x. |
| `cost.other_silicon` | 0 | transistors | C | Silicon outside the register file and the ALU (control, decode, PC adders, memory interface); unaffected by sparing. Swept 0, 6,000, 60,000. |
| `cost.ports` | 2 read ports, 1 write port | ports | C | Ports that need steering or tag comparison. Swept 1 to 3 read ports. |
| `cost.rf_block_transistors` | 144 | transistors | A | An 8-bit register block, the byte slice of this project's register file. |
| `cost.routing_topology` | tagged spare array (default); per-position multiplexers (upper bound) | model | C | Tagged: spares sit in a side array, each holding a tag of the slice it replaces, compared on every access, and the output lane multiplexers gain one input per reaching spare. Per-position: every slice position has its own multiplexer over the spares in reach. |
| `cost.selection_module_transistors` | 27 | transistors | A | A four-input selection module. |
| `cost.shared_block_transistors` | 1152 | transistors | C | Routing shared by all register-file spares (the final select per lane, bit and read port: 8 x 8 x 2 x 9). A single point of failure unless replicated; replication assumes an ideal choice among copies, which is optimistic. |
| `cost.store_bit_transistors` | 18 | transistors | B | Used for the storage part of one tag bit in the spare array. |
| `cost.tag_compare_transistors` | 9 | transistors per tag bit per port | C | One XNOR-class gate. Swept 0 to 27. |
| `cycles.base_per_instruction` | 1 | beats | C | Cost of everything outside the integer ALU: fetch, decode, memory access, the plain PC adders, and M/A/F/D. scripts/m2_cycles.py reports 1, 2 and 4. A larger value dilutes the ALU overhead, so the overhead at 1 is the pessimistic end. |
| `cycles.beats_per_slice_op` | 1 | beats | C | One beat per slice-unit operation, independent of K. A real K-bit adder is slower for larger K; this ignores that. |
| `cycles.regfile_port_matches_alu_slice` | True | bool | C | Assumes operand read and result write stream at the ALU's slice width, so register-file access is hidden behind the ALU beats. For K = 16 or 32 this needs a wider-than-byte read/write port into the byte-slice register file. |
| `cycles.w_form_sign_extension_beats` | 0 | beats | C | Sign-extending a W-form result into the upper slices is done by the write-back path and costs no ALU beat. If it costs one, every W op gets +1. |
| `defects.activation` | 1.0 | probability | C | Probability a defect shows on one access. 1.0 = hard defect (always). Values below 1 model weak/intermittent defects and are swept in the weak-defect experiment. |
| `defects.alu_unit_good_probability` | p_slice_good ** (K / 8) | probability | C | An ALU slice of K bits has the same defect density per bit as an 8-bit register slice. The 0.71 figure is for a register slice, not for logic; applying it to ALU logic is an assumption. |
| `defects.cluster_alpha_sweep` | independent, 50, 20, 9.4, 4, 1 | gamma shape | C | Clustering strengths swept. 50 down to 9.4 span what the wafer map allows at matrix scale; 4 and 1 are stress tests beyond what it allows there, and are hypothetical. |
| `defects.cluster_scale` | blocks of 32 slices, or die-level (one rate for the whole register file) | slices | C | Scales at which a defect-rate multiplier is shared. Die-level is the one the wafer map cannot test and the one that matters most for the spare count. |
| `defects.clustering_model` | gamma-mixed block multipliers; marginal slice yield fixed at p | model | C | Each block of slices draws a defect-rate multiplier ~ Gamma(alpha, mean 1); given it, slices fail independently with probability 1 - exp(-lam * m), with lam chosen so the marginal slice yield is exactly p. |
| `defects.fault_model` | one stuck output bit or stuck carry per defective slice | model | C | The simplest fault a test can be wrong about. Real MoS2 defects are richer (and may cluster); nothing here is calibrated to any device. |
| `fault.alu_upset_sweep` | 5e-7, 2e-6, 8e-6 | 1/(output bit*beat) | C | ALU upset rates swept in M4: each slice-unit output bit (K data bits and the carry) is flipped with this probability per beat, at most one flip per operation. |
| `fault.burst_size` | 1, 2, 3 (memory); 1, 2, 4 (register file) | adjacent bits per event | C | Bits flipped by one event, adjacent within one word (one copy of the register file). No source; chosen to straddle what single-error protection can and cannot handle. |
| `fault.control_state_sweep` | pc 1e-8, 3e-8, 1e-7; instruction 1e-8, 3e-8, 1e-7; csr 3e-9, 1e-8, 3e-8 | 1/(bit*cycle) | C | Upsets in the 64-bit PC, the 32-bit instruction word being fetched (flipped for that one fetch) and five 64-bit CSRs (mstatus, mie, mtvec, mscratch, mepc). Chosen so a run sees about 0.05 to 2 events; not an environment. |
| `fault.infield_defect_sweep` | 1e-9, 3e-9, 1e-8, 1e-7, 3e-7, 1e-6 | 1/(slice*cycle) | C | A stuck-bit defect appears in one register-file slice (primary or spare), after the boot self-test. The literature (docs/results.md, G13 table, rows 9 and 10) says displacement damage and heavy ions can create permanent damage but gives no rate for a circuit. |
| `fault.memory_sweep` | 3e-11, 1e-10, 3e-10 | 1/(bit*cycle) | C | Upsets in memory, per bit of the memory the program touches (its footprint: 349, 253 and 123 64-bit words for coremark_like, chacha20 and irq_loop). Memory the program never uses is not exposed. |
| `fault.model` | independent single-bit flips, Poisson in time | model | C | Register file: one Poisson process over every exposed stored bit (data bits of x1..x31 in every copy, and the parity bit where the scheme has one). ALU: independent per slice-unit call. |
| `fault.rf_upset_sweep` | 1e-9, 3e-9, 1e-8 | 1/(bit*cycle) | C | Register-file upset rates swept in M4 (per exposed stored bit, per model cycle). Chosen so the unprotected core sees roughly 0.3 to 4 upsets per workload run; chosen for statistical usefulness, not because any environment produces them. |
| `fault.seu_per_bit_per_cycle` | 1e-09 | 1/(bit*cycle) | C | Placeholder so that injection code has something to sweep. NOT a measurement of MoS2 or of any environment. Results must be reported as a function of this rate, not at this value. |
| `protection.control_fsm` | triplicated with a voter, not fault-injected | model | C | The control FSM is modelled as triplicated and voted but is not injected, as scoped for v0. Its area and any voter delay are not counted anywhere. |
| `protection.online_retest` | every 500 or 2000 instructions; 8 model cycles per physical slice per copy, or free | instructions | C | The boot self-test and remap re-run while the machine is running. Cost is a stall of 8 cycles per slice per copy (4 patterns x write and read); the free variant isolates the benefit from the stall. |
| `protection.schemes` | rf: none \| parity \| parity-dup \| tmr;  alu: none \| dwc \| tmr;  scrub period | scheme | C | Parity is detect-only (fail-stop). Parity-dup repairs from the clean copy. TMR is a bitwise majority. DWC runs an ALU op twice and a third time on mismatch; ALU TMR runs it three times. The vote, compare and parity logic is fault-free and takes no cycles. |
| `protection.scrub_period` | 32 | instructions per scrubbed word | C | One register word is scrubbed every 32 retired instructions, round-robin; a scrub read costs 1 beat. |
| `regfile.num_regs` | 32 | registers | C | Architectural count. x0 is hardwired to zero in the golden model, so a physical file needs only 31 storage words; the yield tables use 32 x 8 = 256 slices as stated in the project brief. This overstates storage slightly. |
| `regfile.spare_slices` | 0 | slices | C | Shared spare pool size. Swept by the yield model; 0 is the baseline. |
| `regfile.width_bits` | 64 | bits | C | RV64. |
| `run.hang_budget` | 3 x golden instructions + 1000 | instructions | C | A run that has not finished within this many instructions is a hang. |
| `selftest.rounds` | 1 | rounds | C | Boot self-test repetitions. One round is enough for hard defects and far from enough for weak ones (see docs/results.md, M3). Test time and its cost are not modelled. |
| `spares.crossbar` | ideal: any spare serves any slice | model | C | The remap table, its multiplexers and the self-test logic are assumed fault-free and free. Every yield figure is therefore an upper bound. |
| `spares.regions` | 1, 2, 4, 8, 16, 32 regions | regions | C | Reach of a spare: 1 is the ideal pool (any spare serves any slice); n restricts each spare to 1/n of the register file; 32 is one region per register (private spares). |
| `vliw.port_area_frac` | 0.3 | fraction of the one-unit register file | C | Register-file area added per extra unit (two read ports and one write port, as a fraction of the one-unit file). Swept from 0 to 1 in the study; no source gives a value for this process. |
| `vliw.rf_yield_one_unit` | 0.99 | probability | C | Yield of the register file at the one-unit port count, after sparing and repair (the cheapest designs of G10 reach about 0.93 to 0.99). Scaled with area for more ports: y ** (area ratio). |
| `vliw.units` | 4 | units | C | Parallel K-bit units that are also the spares. A design choice, not a measurement. |
| `yield.cluster_alpha_min_matrix_scale` | 9.37 | gamma shape | B | The strongest clustering (smallest gamma shape alpha) that the wafer map does not reject at about 95%: failures inside an 8-register matrix have pairwise correlation of at most about 0.03. It constrains differences BETWEEN matrices only. |
| `yield.implied_per_transistor` | 0.99762 | probability | B | Per-transistor yield that independent failures would need to produce the register yields: 0.71 ** (1/144). The two register yields (8-bit and 64-bit) imply the same value, so yield falls with transistor count as independent failures predict. |
| `yield.measured_64bit_register` | 0.07 | probability | A | "approximately 7% for a 64-bit register with 1,152 transistors". The authors round it; the independent scaling of the 8-bit figure gives 0.71 ** 8 = 0.0646. 1,152 is 8 times 144. |
| `yield.measured_8bit_shift_register` | 0.71 | probability | A | Main text: "The yield of 8-bit registers, each with 144 transistors, reached 71% at the wafer scale." (The supplementary caption says "can reach 71%".) 224 registers on a 4-inch wafer. |
| `yield.measured_8bit_shift_register_from_map` | 0.7366 | probability | A | Counting the map gives 165 working of 224 registers (59 failed), which is 73.7%, not exactly the 71% in the text. The paper does not explain the difference; both are kept. The same map shows no clustering of failures; see docs/results.md. |
| `yield.measured_inverter` | 0.9977 | probability | A | "898 out of 900 inverters worked correctly with a yield of 99.77%": a 30 x 30 array in the 6 mm x 6 mm region of the chip, overall noise tolerance 0.5 V. Two failures in 900 (95% interval 0.06% to 0.81% failing). |
| `yield.measured_transistor` | 0.9992 | probability | A | "The overall transistor yield reached 99.92%": three batches of wafers, each with 7,000 transistors, 800 randomly selected for testing. Isolated-device yield; it does not predict register yield (see yield.implied_per_transistor). |
| `yield.p_slice_good` | 0.71 | probability | B | Probability that one 8-bit register slice is defect-free. Calibrated, not measured, because the modelled slice is assumed to yield like the measured 8-bit shift register of 144 transistors: the same circuit block as the chip's own register, but not a random-ac... |
| `yield.routing_transistor_sweep` | 1, 0.99999, 0.9999, 0.9996, 0.9992, 0.9985, 0.99762 | probability | C | Per-transistor yield of routing and tag logic. 0.9992 is the isolated-device figure (tier A, yield.measured_transistor) and 0.99762 the one implied by the register yields (tier B, yield.implied_per_transistor); the others are intermediate points, e.g. |
| `yield.slice_width_bits` | 8 | bits | C | Granularity of a physical register slice (a byte slice). A modelling choice. |
