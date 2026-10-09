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

```bash
git clone --recurse-submodules https://github.com/ujeshauchitha-glitch/RadSlice.git
bash scripts/run-python-tests.sh
cmake -S . -B build && cmake --build build --target riscv_core
```
