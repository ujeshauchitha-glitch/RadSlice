"""Clustered-defect yield model: the standard negative-binomial (gamma-mixed Poisson) model,
generalised to blocks, computed exactly by numerical integration and convolution.

A physical slice sits in a block. Each block draws a defect-rate multiplier m ~ Gamma(shape alpha,
mean 1); given m, each slice in the block fails independently with probability
q(m) = 1 - exp(-lam * m), and lam is chosen so the MARGINAL slice yield is exactly p:

    E[exp(-lam * m)] = (1 + lam / alpha) ** (-alpha) = p      ->      lam = alpha * (p ** (-1/alpha) - 1)

So changing alpha changes only the correlation between slices, never the average defect density.
Small alpha is strong clustering; alpha = None (or very large) is independence. Blocks are
independent of each other. A block the size of the whole component is die-level clustering.

All of this is tier C (see config/params.toml): it is a what-if model, not a measurement.

    python -m radslice.cluster_model
"""
from __future__ import annotations

import math
from functools import lru_cache

INDEPENDENT_ALPHA = 1e5           # at or above this, treat as independent exactly
_LOWER_CUT = 1e-12                # multipliers below this are treated as q = 0 (q < lam * 1e-12)


def lam_for(p: float, alpha: float | None) -> float:
    if alpha is None or alpha >= INDEPENDENT_ALPHA:
        return -math.log(p)
    return alpha * (p ** (-1.0 / alpha) - 1.0)


def _lower_gamma_mass(alpha: float, x: float) -> float:
    """P(Gamma(alpha) < x) by its power series, for small x."""
    term = x ** alpha * math.exp(-x) / math.gamma(alpha + 1)
    total, n = term, 0
    while term > 1e-18 * total and n < 200:
        n += 1
        term *= x / (alpha + n)
        total += term
    return total


@lru_cache(maxsize=None)
def _grid(alpha: float, points: int = 1400):
    """Quadrature nodes (multipliers m) and weights for Gamma(alpha, mean 1); the weights sum to 1."""
    log_norm = alpha * math.log(alpha) - math.lgamma(alpha)

    def log_g(m):
        return log_norm + (alpha - 1.0) * math.log(m) - alpha * m

    ms, ws = [], []
    if alpha >= 20.0:                                     # narrow: a linear grid around the mean
        sd = 1.0 / math.sqrt(alpha)
        lo, hi = max(1e-9, 1.0 - 12 * sd), 1.0 + 12 * sd
        step = (hi - lo) / (points - 1)
        for i in range(points):
            m = lo + i * step
            w = math.exp(log_g(m)) * step
            if i in (0, points - 1):
                w *= 0.5
            ms.append(m); ws.append(w)
    else:                                                 # wide: a log grid
        m_hi = 1.0 + 12.0 / math.sqrt(alpha) + 60.0 / alpha
        t_lo, t_hi = math.log(_LOWER_CUT), math.log(m_hi)
        step = (t_hi - t_lo) / (points - 1)
        for i in range(points):
            m = math.exp(t_lo + i * step)
            w = math.exp(log_g(m) + t_lo + i * step) * step     # g(m) * m * dt
            if i in (0, points - 1):
                w *= 0.5
            ms.append(m); ws.append(w)
        ms.append(0.0)                                    # mass below the cut: q = 0
        ws.append(_lower_gamma_mass(alpha, alpha * _LOWER_CUT))
    total = sum(ws)
    return tuple(ms), tuple(w / total for w in ws)


def _log_binom_row(n: int):
    return [math.lgamma(n + 1) - math.lgamma(k + 1) - math.lgamma(n - k + 1) for k in range(n + 1)]


@lru_cache(maxsize=None)
def block_pmf(n: int, p: float, alpha: float | None) -> tuple:
    """P(k of the n slices in one block are bad), k = 0..n."""
    if alpha is None or alpha >= INDEPENDENT_ALPHA:
        q = 1.0 - p
        return tuple(math.comb(n, k) * q ** k * p ** (n - k) for k in range(n + 1))
    lam = lam_for(p, alpha)
    ms, ws = _grid(alpha)
    lc = _log_binom_row(n)
    out = [0.0] * (n + 1)
    for m, w in zip(ms, ws):
        if m == 0.0:
            out[0] += w
            continue
        ln_good = -lam * m                                # ln(1 - q), exact
        ln_bad = math.log(-math.expm1(-lam * m)) if lam * m > 1e-300 else -700.0
        for k in range(n + 1):
            out[k] += w * math.exp(lc[k] + k * ln_bad + (n - k) * ln_good)
    return tuple(out)


def _convolve(a, b):
    out = [0.0] * (len(a) + len(b) - 1)
    for i, x in enumerate(a):
        if x == 0.0:
            continue
        for j, y in enumerate(b):
            out[i + j] += x * y
    return out


def _blocks(total: int, block: int | None):
    if not block or block >= total:
        return [total]
    full, rem = divmod(total, block)
    return [block] * full + ([rem] if rem else [])


def bad_count_pmf(total: int, p: float, alpha: float | None, block: int | None):
    """Distribution of the number of bad slices among `total` physical slices in blocks of `block`."""
    pmf = [1.0]
    for size in _blocks(total, block):
        pmf = _convolve(pmf, block_pmf(size, p, alpha))
    return pmf


def pooled_yield(primaries: int, spares: int, p: float, alpha: float | None = None,
                 block: int | None = None) -> float:
    """Ideal shared pool: the chip works if at most `spares` of the primaries+spares slices are bad.

    Only the total number of bad slices matters, so where the spares sit does not matter for an
    ideal pool; only how the slices are grouped into blocks does."""
    pmf = bad_count_pmf(primaries + spares, p, alpha, block)
    return sum(pmf[: spares + 1])


def domain_yield(domains: int, primaries_per_domain: int, spares_per_domain: int, p: float,
                 alpha: float | None = None, block: int | None = None) -> float:
    """Spares restricted to a domain: each domain has its own primaries and spares, laid out
    contiguously, and a spare can only replace a slice of its own domain.

    Exact when blocks and domains are aligned: either every block is a whole number of domains, or
    every domain is a whole number of blocks."""
    d = primaries_per_domain + spares_per_domain
    if alpha is None or alpha >= INDEPENDENT_ALPHA or not block:
        if not block and alpha is not None and alpha < INDEPENDENT_ALPHA:
            block = d * domains                           # die-level: one block for everything
        else:
            one = sum(block_pmf(d, p, None)[: spares_per_domain + 1])
            return one ** domains
    if block >= d:
        if block % d:
            raise ValueError("block must be a whole number of domains, or domains a whole number of blocks")
        per_block = block // d
        total_domains_covered = domains
        lam = lam_for(p, alpha)
        ms, ws = _grid(alpha)
        lc = _log_binom_row(d)
        result = 1.0
        remaining = total_domains_covered
        sizes = []
        while remaining > 0:
            g = min(per_block, remaining)
            sizes.append(g); remaining -= g
        cache = {}
        for g in sizes:
            if g in cache:
                result *= cache[g]
                continue
            acc = 0.0
            for m, w in zip(ms, ws):
                if m == 0.0:
                    acc += w
                    continue
                ln_good = -lam * m
                ln_bad = math.log(-math.expm1(-lam * m)) if lam * m > 1e-300 else -700.0
                cdf = sum(math.exp(lc[k] + k * ln_bad + (d - k) * ln_good) for k in range(spares_per_domain + 1))
                acc += w * min(1.0, cdf) ** g
            cache[g] = acc
            result *= acc
        return result
    if d % block:
        raise ValueError("block must be a whole number of domains, or domains a whole number of blocks")
    pmf = [1.0]
    for _ in range(d // block):
        pmf = _convolve(pmf, block_pmf(block, p, alpha))
    return sum(pmf[: spares_per_domain + 1]) ** domains


def correlation(p: float, alpha: float | None) -> float:
    """Pairwise correlation of two slices' failure indicators inside one block (the intra-class
    correlation). 0 for independence."""
    if alpha is None or alpha >= INDEPENDENT_ALPHA:
        return 0.0
    lam = lam_for(p, alpha)
    return ((1.0 + 2.0 * lam / alpha) ** (-alpha) - p * p) / (p * (1.0 - p))


# ---- what the one published wafer map allows --------------------------------------------------

def loglik(counts: list[int], n: int, p_good: float, alpha: float | None) -> float:
    pmf = block_pmf(n, p_good, alpha)
    return sum(math.log(pmf[c]) for c in counts)


def alpha_lower_bound(counts: list[int], n: int, p_good: float, chi2: float = 2.71) -> float | None:
    """Smallest alpha the data cannot reject at about 95% (one-sided likelihood-ratio test of
    clustering against independence). None if even the strongest clustering searched is allowed."""
    l_inf = loglik(counts, n, p_good, None)
    def excess(a):
        return 2.0 * (l_inf - loglik(counts, n, p_good, a))
    lo, hi = 0.05, 1e4
    if excess(lo) <= chi2:
        return None
    if excess(hi) > chi2:
        return hi
    for _ in range(60):
        mid = math.sqrt(lo * hi)
        if excess(mid) > chi2:
            lo = mid
        else:
            hi = mid
    return hi


def main() -> None:
    from .wafer_map import load_grid
    grid = load_grid()
    counts = [r.count("R") for r in grid]
    p_good = 1 - sum(counts) / (8 * len(counts))
    a = alpha_lower_bound(counts, 8, p_good)
    print(f"wafer map: 28 matrices of 8 registers, marginal yield {p_good:.4f}")
    print(f"data rule out clustering stronger than alpha = {a:.2f} (about 95%, one-sided); "
          f"intra-block correlation at that alpha: {correlation(p_good, a):.3f}")
    print("\nindependent vs clustered yield, 256 primaries, p = 0.71, die-level clustering:")
    print("spares  independent  alpha=4   alpha=1   alpha=0.3")
    for s in (96, 112, 128, 144, 160, 192):
        row = [pooled_yield(256, s, 0.71, a_) for a_ in (None, 4.0, 1.0, 0.3)]
        print(f"{s:6d}  " + "  ".join(f"{y:10.4f}" for y in row))


if __name__ == "__main__":
    main()
