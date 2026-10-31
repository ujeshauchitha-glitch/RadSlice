# RadSlice

A simulation-first research project on a fault-tolerant, bit-sliced RV64I core
for 2D-material (MoS2) chips in radiation environments.

**Nothing here is a measurement.** Every device or fault parameter is in
`config/params.toml` with a tier tag (A published, B calibrated, C hypothetical)
and defaults to C. No novelty is claimed: spare slices exist in silicon. The
research question is the 2D-specific fault model plus combined yield and
radiation handling on a slice-serial RV64 core.

- `docs/gaps.md`: what each feature answers, and what would prove it wrong.
- `docs/results.md`: what was shown, what was not, and what was rejected. Start with its
  "M5" section: eleven conclusions, each with what it rests on and what would overturn it.
- `docs/assumptions.md`: every parameter and its tier, generated from `config/params.toml`.
- `rtl/`: the Verilog for the FPGA target (Terasic DE2-115; see G18 in `docs/gaps.md`).
- `extern/riscv-emu`: the golden model (submodule). Never modified from here.

Layout: `include/radslice` and `src/` hold the hardware model (sliced ALU, sliced
register file); `src/radslice_run.cpp` runs an ELF on the golden core with the model
installed; `tests/` holds the C++ tests; `scripts/` has the drivers.

```bash
git clone --recurse-submodules https://github.com/ujeshauchitha-glitch/RadSlice.git
bash scripts/run-python-tests.sh
cmake -S . -B build && cmake --build build --target riscv_core
```

Hardware-model checks (need a RISC-V gcc on PATH for the last two):

```bash
cmake --build build && ctest --test-dir build -R radslice_
bash scripts/run-hw-riscv-tests.sh 16 rv64ui rv64mi   # K = 8, 16 or 32
python scripts/m2_cycles.py                           # model-estimate cycle overhead
```

Simulated yield against the closed form (needs `radslice_yield`):

```bash
python scripts/m3_yield.py
```

A defective chip under riscv-tests (`RADSLICE_SELFTEST=0` leaves the defects in, as a control):

```bash
RADSLICE_DEFECT_SEED=1 RADSLICE_RF_SPARES=160 RADSLICE_ALU_SPARES=24 \
  bash scripts/run-hw-riscv-tests.sh 16 rv64ui rv64mi
```

Fault-injection campaigns (M4; needs a RISC-V gcc for the workloads):

```bash
bash scripts/build-workloads.sh                  # the three bare-metal workloads
python scripts/m4_campaign.py --quick --out build/m4-quick   # a few minutes; omit --quick for the full 180 cells
build/radslice_inject --elf build/workloads/chacha20.elf --rf tmr --alu dwc --scrub 32 \
  --lambda-rf 3e-9 --lambda-alu 2e-6 --runs 1000 --seed 1
```

Every upset rate is a tier-C parameter, swept and never quoted as a measurement.

Clustered defects (G9; needs `radslice_yield`, about 35 s):

```bash
python scripts/cluster_study.py
```

A five-minute tour for showing the project to someone (about 20 s; needs the build and the workloads):

```bash
python scripts/demo.py
```

Spare-routing cost and the K trade-off (G10; exact models, about 20 s):

```bash
python scripts/crossbar_study.py
python scripts/routing_defect_study.py   # the routing's own defects (G14)
python scripts/logic_style_study.py       # n-only versus complementary logic (G11)
```

More fault classes, seeds and K (G15, G16; needs `radslice_inject` and the workloads, about 6 minutes):

```bash
python scripts/m6_campaign.py
```
