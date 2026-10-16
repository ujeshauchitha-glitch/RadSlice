"""Closed-form yield model for a sliced register file.

Assumptions (all tier C, see config/params.toml and docs/gaps.md G1/G2):
  * Each physical slice is good independently with probability p.
  * Spare slices are as likely to be defective as primary slices.
  * A shared pool is fully flexible: any spare can replace any slice
    (an ideal crossbar). Remap-table and mux area/defects are NOT modelled,
    so shared-pool yields here are upper bounds.
  * Per-register sparing: a spare serves only its own register.
"""
from __future__ import annotations

import random
from math import comb


def binom_tail(n: int, k: int, p: float) -> float:
    """P(Binomial(n, p) >= k), exact summation."""
    if k <= 0:
        return 1.0
    if k > n:
        return 0.0
    q = 1.0 - p
    return sum(comb(n, i) * p**i * q ** (n - i) for i in range(k, n + 1))


def register_yield(slices_per_reg: int, p: float) -> float:
    """One register, no spares: every slice must be good."""
    return p**slices_per_reg


def file_yield_per_register_spares(num_regs: int, slices_per_reg: int,
                                   spares_per_reg: int, p: float) -> float:
    """Every register must be repairable from its own private spares."""
    one = binom_tail(slices_per_reg + spares_per_reg, slices_per_reg, p)
    return one**num_regs


def file_yield_shared_pool(total_slices: int, spares: int, p: float) -> float:
    """Need at least `total_slices` good among total_slices + spares."""
    return binom_tail(total_slices + spares, total_slices, p)


def area_ratio(total_slices: int, spares: int) -> float:
    """Storage-slice count relative to the spare-free file (slices only)."""
    return (total_slices + spares) / total_slices


def mc_shared_pool(total_slices: int, spares: int, p: float,
                   trials: int, seed: int) -> tuple[float, float]:
    """Monte Carlo estimate and its standard error; fixed seed."""
    rng = random.Random(seed)
    n = total_slices + spares
    ok = 0
    for _ in range(trials):
        good = sum(rng.random() < p for _ in range(n))
        ok += good >= total_slices
    est = ok / trials
    se = (est * (1 - est) / trials) ** 0.5
    return est, se


def spare_table(p: float, num_regs: int = 32, slices_per_reg: int = 8,
                pools: tuple[int, ...] = (0, 16, 32, 64, 96, 128)):
    """Rows of (spares, area, per-register yield, shared-pool yield).

    Per-register sparing uses the same total spare count, spread evenly;
    rows where the pool does not divide evenly are skipped for that column.
    """
    total = num_regs * slices_per_reg
    rows = []
    for s in pools:
        per_reg = None
        if s % num_regs == 0:
            per_reg = file_yield_per_register_spares(
                num_regs, slices_per_reg, s // num_regs, p)
        rows.append((s, area_ratio(total, s), per_reg,
                     file_yield_shared_pool(total, s, p)))
    return rows


def format_table(rows) -> str:
    out = ["spares  area   per-register  shared-pool"]
    for s, a, pr, sp in rows:
        pr_s = "n/a" if pr is None else f"{pr:.4f}"
        out.append(f"{s:6d}  {a:4.2f}x  {pr_s:>12}  {sp:.4f}")
    return "\n".join(out)


# ---- ALU (M3) ---------------------------------------------------------------
# An ALU of 64 bits is `64 // k` slice units of k bits. Same defect density per bit
# as the register file: a k-bit unit is good with probability p8 ** (k / 8). Spares
# are interchangeable with any unit (ideal crossbar), so yields are upper bounds.

def alu_units(k: int, xlen: int = 64) -> int:
    return xlen // k


def alu_unit_good_prob(p8: float, k: int) -> float:
    return p8 ** (k / 8)


def alu_yield(k: int, spares: int, p8: float, xlen: int = 64) -> float:
    n = alu_units(k, xlen)
    return binom_tail(n + spares, n, alu_unit_good_prob(p8, k))


def alu_area_ratio(k: int, spares: int, xlen: int = 64) -> float:
    """Slice-unit area relative to the spare-free ALU (a spare is one unit, k bits)."""
    n = alu_units(k, xlen)
    return (n + spares) / n


def min_spares_for(k: int, p8: float, target: float, xlen: int = 64) -> int:
    s = 0
    while alu_yield(k, s, p8, xlen) < target:
        s += 1
    return s


if __name__ == "__main__":
    from .params import load_params

    p = load_params()["yield.p_slice_good"]
    print(f"p_slice_good = {p.value}  (tier {p.tier}: hypothetical)")
    print(f"64-bit register, 0 spares: {register_yield(8, p.value):.4f}")
    print(format_table(spare_table(p.value)))
