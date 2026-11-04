"""G19 step 3, yield: the closed form for the spare-and-remap array, checked against the chips the testbench used.

A chip works if at least P of its P+S elements are free of a defect (the self-test finds each stuck bit with the
two-read test of rtl/tb/tb_mac_selftest.v, and remap uses any healthy element for any lane: an ideal crossbar).
With per-element good probability g:  yield(P, S) = sum_{k >= P} C(P+S, k) g^k (1-g)^(P+S-k).
g is a tier-C parameter: no source gives the defect rate of a MAC element, so it is swept, never quoted as a fact.

Writes results/g19_yield.md. Run from the repository root.
"""
import os
import re
import subprocess
import sys
from math import comb, sqrt

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def yield_cf(P, S, g):
    n = P + S
    return sum(comb(n, k) * g ** k * (1 - g) ** (n - k) for k in range(P, n + 1))


def spares_for(P, g, target):
    S = 0
    while yield_cf(P, S, g) < target:
        S += 1
        if S > 4000:
            return None
    return S


def accepted_in_file(P, S, nchips, q, seed):
    out = subprocess.run([sys.executable, "g19_selftest_vectors.py", str(P), str(S), str(nchips), str(q), str(seed), "2"],
                         cwd=os.path.join(ROOT, "scripts"), capture_output=True, text=True, check=True).stdout
    return len(re.findall(r"^C 1$", out, flags=re.M))


def main():
    lines = ["# G19 step 3: yield of the spare-and-remap MAC array", ""]
    lines += ["## The closed form against the chips the testbench used", "",
              "Each element defective with probability q (a stuck bit); a chip is accepted if at most S elements are "
              "defective. `accepted` is read from the chip file whose accept/reject decisions the Verilog self-test "
              "reproduced exactly (0 decision mismatches in `run-mac-selftest.sh`).", "",
              "| P | S | q | chips | accepted | closed form | z |", "|---|---|---|---|---|---|---|"]
    for P, S, q in [(8, 4, 0.25), (8, 2, 0.15), (4, 4, 0.30)]:
        n = 150
        acc = accepted_in_file(P, S, n, q, 5)
        y = yield_cf(P, S, 1 - q)
        z = (acc - n * y) / sqrt(n * y * (1 - y))
        lines.append(f"| {P} | {S} | {q} | {n} | {acc} | {y * n:.1f} ({y:.3f}) | {z:+.2f} |")
    lines += ["", "## Spares needed for a 99% yield (closed form)", "",
              "| per-element good probability g | P = 8 | P = 32 | P = 128 | P = 512 |", "|---|---|---|---|---|"]
    for g in (0.99, 0.95, 0.9, 0.8, 0.7):
        row = []
        for P in (8, 32, 128, 512):
            S = spares_for(P, g, 0.99)
            row.append("none within 4000" if S is None else f"{S} ({(P + S) / P:.2f}x elements)")
        lines.append(f"| {g} | " + " | ".join(row) + " |")
    lines += ["", "All g are tier C. Reading the table: the spare overhead FALLS as the array gets wider (more elements average "
              "out the defects) and RISES as g drops, from about 2% to 12% extra elements at g = 0.99 to about 50% to 125% at "
              "g = 0.7. The per-element defect rate, which is not measured here, therefore decides whether sparing alone "
              "is cheap or not.", ""]
    with open(os.path.join(ROOT, "results", "g19_yield.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print("\n".join(lines))


if __name__ == "__main__":
    main()
