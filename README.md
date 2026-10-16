# RadSlice

A simulation-first research project on a fault-tolerant, bit-sliced RV64I core
for 2D-material (MoS2) chips in radiation environments.

**Nothing here is a measurement.** Every device or fault parameter is in
`config/params.toml` with a tier tag (A published, B calibrated, C hypothetical)
and defaults to C. No novelty is claimed: spare slices exist in silicon. The
research question is the 2D-specific fault model plus combined yield and
radiation handling on a slice-serial RV64 core.

- `docs/gaps.md`: what each feature answers, and what would prove it wrong.
- `docs/results.md`: what was shown, what was not, and what was rejected.
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
