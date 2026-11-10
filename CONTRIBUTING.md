# Contributing

RadSlice is a one-person study at the moment, but it is written so that someone else can check it.
If you want to rerun, extend or correct it, these are the rules the repository follows.

## Rules that every change keeps
- **Nothing is a measurement.** Every device or fault parameter lives in `config/params.toml` with a tier
  (A published, B calibrated, C hypothetical) and a `source` for A and B. A new parameter defaults to C.
  `docs/assumptions.md` is generated from that file (`python -m radslice.assumptions --write`) and a test
  fails if it drifts.
- **Numbers come from runs.** Do not write a number into a document that a script in this repository
  did not produce. Name the script and, where it exists, the result file.
- **Predictions before results.** A study states its claims and what would prove them wrong in
  `docs/gaps.md` (or a preregistration file such as `docs/preregistration-g20.md`) in a commit that
  precedes the run.
- **Failures stay.** Wrong predictions, rejected ideas and corrections are recorded in `docs/results/`,
  not deleted.
- **No novelty claims, no radiation claims.** Prior-art checks are still outstanding; nothing here says
  that any 2D-material circuit is radiation resistant.
- **Commit messages** describe the change in plain words and carry no tool attribution.

## Before a pull request
```bash
bash scripts/run-python-tests.sh      # 79 tests, about 5 s
make reproduce                        # G19 step 4 and G20 regenerated, about 2 minutes
```
If you touched the C++ model: `cmake -S . -B build && cmake --build build -j && ctest --test-dir build -R radslice_`.
If you touched the Verilog: `bash scripts/run-rtl-tests.sh`, `bash scripts/run-mac-tests.sh`,
`bash scripts/run-mac-selftest.sh` (Icarus Verilog on PATH).

A result file under `results/` is committed with the script that wrote it; if the numbers change, the
matching section of `docs/results/` says what changed and why.

## Layout
See the top of `README.md` and `docs/README.md`.
