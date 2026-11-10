# RadSlice

**The question.** For a small int8 classifier whose weights and accumulators suffer bit upsets, how much accuracy
does each cheap protection (scrubbing, SECDED ECC, a checksum lane, duplication, triple voting, reload from a
clean copy) keep, at what cost, and which of them fail in ways a quick analysis would miss?

Everything here is simulation under stated, swept assumptions. **Nothing is a measurement of a chip or of any
upset rate.** No novelty is claimed (prior-art checks are still outstanding, see `docs/planning/council-2-direction.md`),
and no claim is made that any 2D-material circuit is radiation resistant.

## What is verified (each by a check that can fail, and a mutant that makes it fail)
- A Verilog int8 MAC array with spare elements (`rtl/mac_array.v`) matches an integer reference bit for bit on
  1,760 random and real-network tiles at four sizes, 0 mismatches; a deliberately stuck element is caught on
  every tile (`bash scripts/run-mac-tests.sh`, `docs/results/g19-mac-array.md`).
- On 450 random defective arrays, a boot self-test plus remap in Verilog reproduces a separately written model's
  accept/reject decisions and repaired outputs exactly (121, 122 and 140 of 150 chips accepted at three sizes),
  and the accept rate matches the closed-form yield (|z| <= 1.2). Removing half the self-test produces 27 wrong
  decisions (`bash scripts/run-mac-selftest.sh`).
- A slice-serial RV64I core in Verilog matches the golden emulator instruction by instruction on two workloads at
  K = 8, 16, 32, and its ALU and register file match the C++ model on 27,935 and 60,000 vectors each
  (`bash scripts/run-rtl-tests.sh`, `bash scripts/run-core-lockstep.sh`, `docs/results/g18-fpga-core.md`). The
  design compiles in Quartus for a DE2-115 (16% of the device) and has **not** been run on a board.
- 79 Python tests, 8 C++ test suites and riscv-tests (rv64ui + rv64mi, 65/65 at every K) pass locally; the GitHub Actions
  jobs in `.github/workflows/ci.yml` passed on each of the six most recent runs (2026-11-04 to 2026-11-06, checked
  through the public API on 2026-11-10).

## Headline results, with their limits
All from `python scripts/g19_campaign.py` and `python scripts/g20_weight_protection.py`, 2,700 images per cell on a
64-32-10 digit classifier (fault-free integer accuracy 0.9685; "still works" means >= 0.9585). Rates are tier-C
placeholders swept over decades, never measured.

1. **Accumulator upsets** (`results/g19_campaign.md`): with no protection accuracy holds the floor up to 1e-5 per
   element per cycle; duplicate-and-compare and the checksum lane hold it to 1e-4 (at 2x and 1.125x the elements);
   triple voting to 1e-3 (3x). A hypothesis stated before the run, that the checksum lane would dominate, was
   wrong. The retry schemes collapse once nearly every pass sees an upset (accuracy 0.25 and 0.66 at 1e-3).
2. **Weight upsets** (`results/g19_campaign.md`): voting and duplication on the accumulators do nothing, because all
   copies read the same corrupted weights. Only the checksum lane detects them, and it only helps **with a clean
   copy to reload from** (full accuracy at every rate swept, up to 2x cycles); without one it is the worst scheme
   (0.844 at 1e-5 per bit per image).
3. **Weight-memory protection, preregistered** (`docs/preregistration-g20.md`, `results/g20_weight_protection.md`):
   three of six predictions held and three were wrong. ECC without a restore is the worst protected option
   (accuracy 0.663 at 1e-6 over a 2,700-image mission, because errors accumulate and any uncorrectable word fails
   the image); plain scrubbing every 10 images holds the floor to 1e-4 with no extra memory; checksum plus reload
   and ECC plus reload both keep full accuracy, trading reload frequency against 1.625x memory. The predicted
   miscorrection by ECC at high rates was not observed.

Limits that apply to all three: voters, comparators, checksum adders, ECC logic and the reload path are fault-free;
single independent bit flips only; a 2,368-weight network, not a workload anyone deploys; the clean copy is immune;
consecutive images are treated as independent; no area, timing or power of the protection logic; standard error
about 0.3 accuracy points per cell. Rerunning the campaigns on 2026-11-10 reproduced every committed table exactly.

## Reproduce in one command
```bash
git clone https://github.com/ujeshauchitha-glitch/RadSlice.git && cd RadSlice
pip install -r requirements.txt
make reproduce          # G19 step 4 and G20, about 2 minutes with Python >= 3.11; then: git status results/
make test               # 79 Python tests, about 5 s
```
On Windows, `mingw32-make reproduce` works (MinGW); if the first `python` on PATH lacks numpy, pass `PYTHON=py`, or
run the four `python scripts/...` lines listed in the `Makefile` by hand.
The Verilog checks need Icarus Verilog on PATH; the C++ model and the riscv-tests need CMake, a C++20 compiler and
the submodule (`git submodule update --init`; `cmake -S . -B build && cmake --build build -j`;
`ctest --test-dir build -R radslice_`); the workloads and core lockstep need a RISC-V gcc. Every other study has
its command in `docs/results/summary.md` under "Reproducing everything".

## Status and limits
- **Track A (current):** the simulation studies above are done. The next step the plan calls for is the same fault
  list run three ways (software injection, Verilog, the DE2-115 board) to see where software-level injection
  mispredicts; nothing has run on the board yet, and the MRFI port has not started (`docs/goals.md`,
  `docs/planning/council-2-direction.md`).
- **Track B (long shot, paused):** G1 to G18 studied spare slices, boot self-test, remap and protection for a
  slice-serial RV64I core under a defect model calibrated only to two published MoS2 register yields (8-bit 71%,
  64-bit about 7%, Ao, Zhou et al., Nature 640, 2025). Of 57 parameters in `config/params.toml`, 13 have a source
  and 44 are hypothetical (`docs/assumptions.md`). An offer to rerun it with measured data is drafted in
  `docs/planning/collaboration-2d.md` and has not been sent.
- Wrong predictions and rejected ideas are kept in `docs/results/summary.md` and in each study's section.
- An independent audit of the repository (structure, use case, credibility, reproducibility) is in
  `docs/audit-2026-11.md`; the honest view of money, funding and whom to approach is in `docs/path-forward.md`.

## Where the open questions are
- `docs/gaps.md`: every study's claims and what would prove them wrong; `docs/results/summary.md` "Open questions".
- Outstanding before any claim of novelty: the prior-art checks listed in `docs/planning/council-2-direction.md`.
- Decisions only the owner can make: a faculty sponsor, the fellowship application (deadline 30 November 2026),
  whether to install MRFI, and the board cable and bench time (`docs/path-forward.md`).

## Layout
```
config/params.toml     every device and fault parameter, tiered A/B/C with sources
python/radslice/       closed-form yield, clustering, cost and routing models; assumptions generator; tests in python/tests
scripts/               study drivers (g19_*, g20_*, m*_*, *_study.py) and the Verilog/riscv-tests runners (run-*.sh)
rtl/                   Verilog: MAC array and self-test testbench, slice-serial ALU, register file, RV64I core, DE2-115 wrapper
include/ src/ tests/   the C++ hardware model installed beneath the golden emulator, and its tools and tests (CMake)
extern/riscv-emu       the golden RV64 emulator, a submodule pinned to the commit with the RadSlice hooks; never edited here
workloads/             bare-metal RV64I programs for the fault-injection campaigns
results/               every table and figure a document quotes, written by the scripts
docs/                  see docs/README.md
```
Licence: MIT (`LICENSE`). Cite with `CITATION.cff`. Contributing rules: `CONTRIBUTING.md`.
