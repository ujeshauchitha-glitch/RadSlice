# Assumptions register

Generated from `config/params.toml` by `python -m radslice.assumptions --write`; do not edit by
hand (a test fails if this file drifts from the parameters).

Tiers: **A** published measurement (source given), **B** calibrated model (source given),
**C** hypothetical. A simulation that uses any tier-C value is a what-if, not a measurement.

**26 parameters: 2 tier A, 2 tier B, 22 tier C.**

4 parameters have a source: `yield.implied_per_transistor`, `yield.measured_8bit_shift_register`, `yield.measured_8bit_shift_register_from_map`, `yield.p_slice_good`. All come from one paper's Supplementary Information (Supplementary Fig. 7); its main text was not read. Every other parameter is tier C, and every result in `docs/results.md` is still conditional on those.

| sourced parameter | source |
|---|---|
| `yield.implied_per_transistor` | Derived from 0.71 over 144 transistors (Supp. Fig. 7(c)); 0.99788 from the map count |
| `yield.measured_8bit_shift_register` | Ao, Zhou et al., Nature 640, 654-661 (2025), Supplementary Information, Supplementary Fig. 7(c) caption |
| `yield.measured_8bit_shift_register_from_map` | Same figure, panel (c): the wafer map, transcribed by scripts/si_fig7_extract.py into results/si_fig7_wafer_map.txt |
| `yield.p_slice_good` | Calibrated to the measured 8-bit shift-register yield above (Nature 640, 654-661, Supp. Fig. 7(c)); the lower of the two readings is used |

| parameter | value | unit | tier | note |
|---|---|---|---|---|
| `alu.slice_width_bits` | 16 | bits | C | K. Swept over 8, 16, 32. Each ALU op takes 64/K beats. |
| `cycles.base_per_instruction` | 1 | beats | C | Cost of everything outside the integer ALU: fetch, decode, memory access, the plain PC adders, and M/A/F/D. scripts/m2_cycles.py reports 1, 2 and 4. A larger value dilutes the ALU overhead, so the overhead at 1 is the pessimistic end. |
| `cycles.beats_per_slice_op` | 1 | beats | C | One beat per slice-unit operation, independent of K. A real K-bit adder is slower for larger K; this ignores that. |
| `cycles.regfile_port_matches_alu_slice` | True | bool | C | Assumes operand read and result write stream at the ALU's slice width, so register-file access is hidden behind the ALU beats. For K = 16 or 32 this needs a wider-than-byte read/write port into the byte-slice register file. |
| `cycles.w_form_sign_extension_beats` | 0 | beats | C | Sign-extending a W-form result into the upper slices is done by the write-back path and costs no ALU beat. If it costs one, every W op gets +1. |
| `defects.activation` | 1.0 | probability | C | Probability a defect shows on one access. 1.0 = hard defect (always). Values below 1 model weak/intermittent defects and are swept in the weak-defect experiment. |
| `defects.alu_unit_good_probability` | p_slice_good ** (K / 8) | probability | C | An ALU slice of K bits has the same defect density per bit as an 8-bit register slice. The 0.71 figure is for a register slice, not for logic; applying it to ALU logic is an assumption. |
| `defects.fault_model` | one stuck output bit or stuck carry per defective slice | model | C | The simplest fault a test can be wrong about. Real MoS2 defects are richer (and may cluster); nothing here is calibrated to any device. |
| `fault.alu_upset_sweep` | 5e-7, 2e-6, 8e-6 | 1/(output bit*beat) | C | ALU upset rates swept in M4: each slice-unit output bit (K data bits and the carry) is flipped with this probability per beat, at most one flip per operation. |
| `fault.model` | independent single-bit flips, Poisson in time | model | C | Register file: one Poisson process over every exposed stored bit (data bits of x1..x31 in every copy, and the parity bit where the scheme has one). ALU: independent per slice-unit call. |
| `fault.rf_upset_sweep` | 1e-9, 3e-9, 1e-8 | 1/(bit*cycle) | C | Register-file upset rates swept in M4 (per exposed stored bit, per model cycle). Chosen so the unprotected core sees roughly 0.3 to 4 upsets per workload run; chosen for statistical usefulness, not because any environment produces them. |
| `fault.seu_per_bit_per_cycle` | 1e-09 | 1/(bit*cycle) | C | Placeholder so that injection code has something to sweep. NOT a measurement of MoS2 or of any environment. Results must be reported as a function of this rate, not at this value. |
| `protection.control_fsm` | triplicated with a voter, not fault-injected | model | C | The control FSM is modelled as triplicated and voted but is not injected, as scoped for v0. Its area and any voter delay are not counted anywhere. |
| `protection.schemes` | rf: none \| parity \| parity-dup \| tmr;  alu: none \| dwc \| tmr;  scrub period | scheme | C | Parity is detect-only (fail-stop). Parity-dup repairs from the clean copy. TMR is a bitwise majority. DWC runs an ALU op twice and a third time on mismatch; ALU TMR runs it three times. The vote, compare and parity logic is fault-free and takes no cycles. |
| `protection.scrub_period` | 32 | instructions per scrubbed word | C | One register word is scrubbed every 32 retired instructions, round-robin; a scrub read costs 1 beat. |
| `regfile.num_regs` | 32 | registers | C | Architectural count. x0 is hardwired to zero in the golden model, so a physical file needs only 31 storage words; the yield tables use 32 x 8 = 256 slices as stated in the project brief. This overstates storage slightly. |
| `regfile.spare_slices` | 0 | slices | C | Shared spare pool size. Swept by the yield model; 0 is the baseline. |
| `regfile.width_bits` | 64 | bits | C | RV64. |
| `run.hang_budget` | 3 x golden instructions + 1000 | instructions | C | A run that has not finished within this many instructions is a hang. |
| `selftest.rounds` | 1 | rounds | C | Boot self-test repetitions. One round is enough for hard defects and far from enough for weak ones (see docs/results.md, M3). Test time and its cost are not modelled. |
| `spares.crossbar` | ideal: any spare serves any slice | model | C | The remap table, its multiplexers and the self-test logic are assumed fault-free and free. Every yield figure is therefore an upper bound. |
| `yield.implied_per_transistor` | 0.99762 | probability | B | Per-transistor yield implied by the register measurement if transistors fail independently: 0.71 ** (1/144). Useful for sizing other circuits by transistor count. |
| `yield.measured_8bit_shift_register` | 0.71 | probability | A | The caption says the yield of 8-bit shift registers, each of 144 transistors, "can reach 71% on the wafer" (224 registers on a 4-inch wafer). "Can reach" may mean a best case. This is a measurement of an 8-bit MoS2 shift register, not of a register-file slice. |
| `yield.measured_8bit_shift_register_from_map` | 0.7366 | probability | A | Counting the map gives 165 working of 224 registers (59 failed), which is 73.7%, not exactly the caption's 71%. The SI does not say which definition the caption uses; both are kept. |
| `yield.p_slice_good` | 0.71 | probability | B | Probability that one 8-bit register slice is defect-free. The assumption behind calling this calibrated rather than measured: a byte slice of the modelled register file yields like the measured 8-bit shift register of 144 transistors. |
| `yield.slice_width_bits` | 8 | bits | C | Granularity of a physical register slice (a byte slice). A modelling choice. |
