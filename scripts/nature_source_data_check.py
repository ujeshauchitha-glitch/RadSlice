"""Check device-level yield from the Nature paper's Fig. 2f source data.

Source data for Ao, Zhou et al., Nature 640, 654-661 (2025), doi:10.1038/s41586-025-08759-9,
file "41586_2025_8759_MOESM3_ESM.zip" (Fig2_f.xlsx: voltage transfer curves of MoS2 NMOS
inverters at Vdd = 4 V). The data are not redistributed here; pass your own copy.
Requires numpy and openpyxl (not needed by the tests).

    python scripts/nature_source_data_check.py path/to/41586_2025_8759_MOESM3_ESM.zip

What it does, and what it cannot do. "Functional" is THIS SCRIPT's definition, not the
paper's: full rail-to-rail swing, V_OH > 3.5 V at Vin = 0 and V_OL below a threshold at
Vin = 4 V. The column order of the file is not documented, so any pattern in the order is
reported as a pattern in the file and not as a physical location.
"""
import math
import random
import sys
import tempfile
import zipfile
from pathlib import Path


def wilson(k, n, z=1.96):
    p = k / n
    d = 1 + z * z / n
    c = (p + z * z / (2 * n)) / d
    h = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / d
    return c - h, c + h


def max_in_window(positions, w):
    s = sorted(positions)
    best, j = 0, 0
    for i in range(len(s)):
        while s[i] - s[j] >= w:
            j += 1
        best = max(best, i - j + 1)
    return best


def main():
    import numpy as np
    import openpyxl

    src = Path(sys.argv[1])
    if src.suffix.lower() == ".zip":
        tmp = tempfile.mkdtemp(prefix="nature_moesm3_")
        with zipfile.ZipFile(src) as z:
            member = next(m for m in z.namelist() if m.endswith("Fig2_f.xlsx"))
            z.extract(member, tmp)
            path = Path(tmp) / member
    else:
        path = src
    ws = openpyxl.load_workbook(path, data_only=True).worksheets[0]
    rows = [list(r) for r in ws.iter_rows(values_only=True)][2:]
    vin = np.array([r[0] for r in rows], float)
    cols = np.array([[np.nan if v is None else float(v) for v in r[1:]] for r in rows])
    c = cols[:, ~np.isnan(cols).any(axis=0)]
    n = c.shape[1]
    voh, vol = c[0], c[-1]
    distinct = len({tuple(np.round(c[:, j], 4)) for j in range(n)})
    print(f"{n} transfer curves with data ({distinct} distinct); Vdd = 4 V")

    print("\nthreshold sensitivity (V_OH > 3.5 V required):")
    for t in (0.3, 0.5, 0.8, 1.0, 1.5):
        k = int((~((voh > 3.5) & (vol < t))).sum())
        lo, hi = wilson(k, n)
        print(f"  V_OL < {t:.1f} V: {k:3d} of {n} fail = {100 * k / n:.2f}% (95% CI {100 * lo:.2f}-{100 * hi:.2f}%)")

    fail = ~((voh > 3.5) & (vol < 0.5))
    k = int(fail.sum())
    lo, hi = wilson(k, n)
    pt = lambda f: 1 - (1 - f) ** 0.5            # an inverter has 2 transistors
    print(f"\nper-transistor failure implied at V_OL < 0.5 V: {100 * pt(k / n):.2f}% "
          f"(CI {100 * pt(lo):.2f}-{100 * pt(hi):.2f}%)")
    print(f"per-transistor failure implied by the register yield (144 transistors): "
          f"{100 * (1 - 0.71 ** (1 / 144)):.2f}% from 71%, {100 * (1 - 0.7366 ** (1 / 144)):.2f}% from 73.7%")

    bad = (np.where(fail)[0] + 1).tolist()
    print("\nfailing curve indices (column order in the file):", bad)
    obs = max_in_window(bad, 21)
    rng, reps, hits = random.Random(7), 20000, 0
    for _ in range(reps):
        hits += max_in_window(rng.sample(range(1, n + 1), k), 21) >= obs
    print(f"most failures in any 21 consecutive curves: {obs}; "
          f"P(at least that, failures placed at random) = {hits / reps:.4f}")


if __name__ == "__main__":
    main()
