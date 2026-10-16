"""Analysis of the published wafer map of 8-bit MoS2 shift registers.

Source: Ao, Zhou et al., "A RISC-V 32-bit microprocessor based on two-dimensional
semiconductors", Nature 640, 654-661 (2025), Supplementary Information, Supplementary
Fig. 7. The caption describes a 4-inch wafer with 28 matrices of 64-bit MoS2 registers,
each matrix being 8 parallel 8-bit shift registers (224 in all, 144 transistors each),
and says the yield of the 8-bit registers "can reach 71%". Panel (c) of the figure is a
map with one green ("Work") or red ("Error") stripe per 8-bit register.

`results/si_fig7_wafer_map.txt` is that map transcribed from the figure by
`scripts/si_fig7_extract.py`: one line per matrix (28), one character per register
(8), G = works, R = error, in the figure's top-to-bottom order. It is a table of 224
pass/fail facts, not a copy of the figure. The paper itself is behind a paywall and is
not redistributed here.

Everything is pure Python with a fixed seed, so the numbers reproduce exactly.

    python -m radslice.wafer_map
"""
from __future__ import annotations

import random
from math import comb
from pathlib import Path

MAP_PATH = Path(__file__).resolve().parents[2] / "results" / "si_fig7_wafer_map.txt"
TRANSISTORS_PER_REGISTER = 144          # from the caption
CAPTION_YIELD = 0.71                    # "can reach 71% on the wafer" (caption)
# Figure rows are laid out in bands of matrices, top to bottom (from the figure).
BAND_SIZES = (4, 4, 6, 6, 4, 4)


def load_grid(path: Path = MAP_PATH) -> list[str]:
    rows = [r.strip() for r in Path(path).read_text().split("\n") if r.strip()]
    if len(rows) != 28 or any(len(r) != 8 or set(r) - {"G", "R"} for r in rows):
        raise ValueError("expected 28 matrices of 8 registers, each G or R")
    return rows


def binom_cdf(k: int, n: int, p: float) -> float:
    """P(X <= k) for X ~ Binomial(n, p)."""
    return sum(comb(n, i) * p**i * (1 - p) ** (n - i) for i in range(0, k + 1))


def summarize(grid: list[str]) -> dict:
    n_mat, width = len(grid), len(grid[0])
    failed = [row.count("R") for row in grid]
    total, bad = n_mat * width, sum(failed)
    work = total - bad
    p = work / total
    return dict(
        matrices=n_mat, registers=total, failed=bad, works=work, pass_rate=p,
        failures_per_matrix=failed,
        perfect_matrices=sum(1 for f in failed if f == 0),
        expected_perfect_if_independent=n_mat * p**width,
        row_failure_rate=[sum(1 for row in grid if row[i] == "R") / n_mat for i in range(width)],
        implied_per_transistor_yield=p ** (1 / TRANSISTORS_PER_REGISTER),
        caption_per_transistor_yield=CAPTION_YIELD ** (1 / TRANSISTORS_PER_REGISTER),
    )


def _sample_vars_and_adjacent(n_mat: int, width: int, pe: float, reps: int, rng: random.Random):
    vars_, adj = [], []
    for _ in range(reps):
        counts, pairs = [], 0
        for _m in range(n_mat):
            row = [rng.random() < pe for _ in range(width)]
            counts.append(sum(row))
            pairs += sum(1 for i in range(width - 1) if row[i] and row[i + 1])
        mean = sum(counts) / n_mat
        vars_.append(sum((c - mean) ** 2 for c in counts) / (n_mat - 1))
        adj.append(pairs)
    return vars_, adj


def independence_tests(grid: list[str], reps: int = 20000, seed: int = 12345) -> dict:
    """Compare the map with independent failures at the observed failure rate.

    - dispersion of failures per matrix: more than binomial means clustered failures
      (a bad matrix), less means failures spread more evenly than chance;
    - adjacent failed registers within a matrix;
    - the failure rate of the first (top) register of a matrix, against the rest.
    """
    n_mat, width = len(grid), len(grid[0])
    pe = sum(r.count("R") for r in grid) / (n_mat * width)
    obs_counts = [r.count("R") for r in grid]
    mean = sum(obs_counts) / n_mat
    obs_var = sum((c - mean) ** 2 for c in obs_counts) / (n_mat - 1)
    obs_adj = sum(1 for r in grid for i in range(width - 1) if r[i] == "R" and r[i + 1] == "R")
    vars_, adj = _sample_vars_and_adjacent(n_mat, width, pe, reps, random.Random(seed))
    row0_failed = sum(1 for r in grid if r[0] == "R")
    return dict(
        failure_rate=pe,
        observed_variance=obs_var, binomial_variance=width * pe * (1 - pe),
        p_variance_at_least=sum(v >= obs_var for v in vars_) / reps,
        p_variance_at_most=sum(v <= obs_var for v in vars_) / reps,
        adjacent_failed_pairs=obs_adj, adjacent_expected=n_mat * (width - 1) * pe * pe,
        p_adjacent_at_least=sum(a >= obs_adj for a in adj) / reps,
        row0_failed=row0_failed, row0_expected=n_mat * pe,
        p_row0_at_most=binom_cdf(row0_failed, n_mat, pe),
    )


def band_failure_rates(grid: list[str]) -> list[float]:
    out, i = [], 0
    for s in BAND_SIZES:
        block = grid[i:i + s]
        out.append(sum(r.count("R") for r in block) / (8 * s))
        i += s
    return out


def main() -> None:
    grid = load_grid()
    s = summarize(grid)
    t = independence_tests(grid)
    print(f"{s['registers']} registers in {s['matrices']} matrices: {s['works']} work, {s['failed']} fail")
    print(f"pass rate from the map: {s['pass_rate']:.4f}   (caption: {CAPTION_YIELD})")
    print(f"implied per-transistor yield over {TRANSISTORS_PER_REGISTER} transistors: "
          f"{s['implied_per_transistor_yield']:.5f} (map) / {s['caption_per_transistor_yield']:.5f} (caption)")
    print(f"matrices with all 8 registers working: {s['perfect_matrices']} of {s['matrices']} "
          f"({100 * s['perfect_matrices'] / s['matrices']:.1f}%), expected if independent "
          f"{s['expected_perfect_if_independent']:.1f}")
    print("failures per matrix:", s["failures_per_matrix"])
    print(f"variance of failures per matrix: {t['observed_variance']:.2f} vs {t['binomial_variance']:.2f} "
          f"if independent; P(>=) = {t['p_variance_at_least']:.3f}, P(<=) = {t['p_variance_at_most']:.3f}")
    print(f"adjacent failed pairs: {t['adjacent_failed_pairs']} vs {t['adjacent_expected']:.1f} expected; "
          f"P(>=) = {t['p_adjacent_at_least']:.3f}")
    print("failure rate by register position 0..7:", [round(x, 2) for x in s["row_failure_rate"]])
    print(f"first register failed in {t['row0_failed']} of {s['matrices']} matrices "
          f"(expected {t['row0_expected']:.1f}); P(<=) = {t['p_row0_at_most']:.4f}")
    print("failure rate by wafer band, top to bottom:", [round(x, 3) for x in band_failure_rates(grid)])


if __name__ == "__main__":
    main()
