"""G11: does the logic style (n-type only versus complementary) change the cost and yield picture?

The paper's own library is n-type only: an enhancement-and-depletion-mode ratioed inverter with one load
transistor, and gates built from a driver network plus a single load (main text p. 656-657: "the entire
RV32-WUJI chip uses only n-type MoS2 transistors"). Every transistor count used in G10 and G14 comes from
that library, so the model so far is an n-only model.

A complementary static gate with n inputs has 2n transistors; an n-only ratioed gate has n + 1 (n drivers and
one load). The ratio is 4/3 for a 2-input gate, 8/5 for a 4-input gate and tends to 2. Here a style
multiplier mu on every transistor count stands for that (tier C, swept over 1, 4/3, 8/5, 2; 1 = the paper's
n-only style). The multiplier does two things:
  * silicon grows by mu (bare register file and everything else);
  * a function made of mu times as many transistors yields p ** mu if the per-transistor yield is unchanged,
    and routing at per-transistor yield q yields q ** mu per same function.
It does not model timing, static power (a ratioed gate burns static power, a complementary one does not),
or a different per-transistor yield for p-type devices, which would be a further tier-C factor.

Usage (repo root):   python scripts/logic_style_study.py [--out DIR]
"""
import argparse
import csv
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python"))
sys.path.insert(0, str(ROOT / "scripts"))
from radslice import cost_model as cost  # noqa: E402
from radslice import routing_model as rm  # noqa: E402
from radslice.cost_model import Costs  # noqa: E402
from radslice.params import load_params  # noqa: E402
import routing_defect_study as rds  # noqa: E402

PRM = load_params()
P0 = PRM["yield.p_slice_good"].value
Q_REG = PRM["yield.implied_per_transistor"].value
Q_DEV = PRM["yield.measured_transistor"].value
MUS = (1.0, 4 / 3, 8 / 5, 2.0)
BASE = cost.RF_PRIMARIES * cost.T_RF_BLOCK
KS = (1, 2, 4, 8, 16, 32)


def best_rf(p, q, mu_unused=1.0):
    """Cheapest register file (tagged, spares' private routing only) at slice yield p, routing yield q."""
    c = Costs()
    best = None
    for n in rds.REGIONS:
        for s in range(n, 1201, n if n > 1 else 1):
            y = rm.rf_core_yield(p, q, s, n)
            if y < 1e-6:
                continue
            t = rm.rf_silicon_T(s, n, 1, c, shared_T=0.0)
            g = t / y / BASE
            if best is None or g < best["per_good"]:
                best = dict(regions=n, spares=s, yield_=y, silicon_rel=t / BASE, per_good=g)
    return best


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "results"))
    out = Path(ap.parse_args().out)
    out.mkdir(parents=True, exist_ok=True)
    rows, md = [], []
    md.append("# G11: n-type only versus complementary logic\n")
    md.append("The paper's chip is n-type only with enhancement/depletion ratioed logic (main text p. 656-657), so "
              "mu = 1 is the style every earlier number assumed. mu scales every transistor count (a complementary "
              "n-input gate has 2n transistors, a ratioed n-only gate n + 1: 4/3 for 2 inputs, 8/5 for 4, towards 2). "
              "A block of mu times the transistors yields p^mu at an unchanged per-transistor yield; routing yields "
              "q^mu per same function. Nothing here models timing, static power, or a different yield for p-type devices.\n")
    md.append("## E1. The cheapest register file against mu\n")
    md.append("Tagged routing, spares' private routing counted, no shared block (as in G14, first row group). "
              "`silicon` is the absolute silicon relative to the n-only bare register file (mu x the relative figure).\n")
    md.append("| mu | slice yield p^mu | routing q | regions | spares | yield | cost per good (own bare file = 1) | "
              "absolute silicon per good, n-only bare file = 1 |")
    md.append("|---:|---:|:--|---:|---:|---:|---:|---:|")
    for mu in MUS:
        p = P0 ** mu
        for qname, q0 in (("none (fault-free)", 1.0), ("0.9992 (isolated device)", Q_DEV), ("0.99762 (register-implied)", Q_REG)):
            q = q0 ** mu
            b = best_rf(p, q)
            if b is None:
                md.append(f"| {mu:.2f} | {p:.3f} | {qname} | none | none | below 1e-6 | not reachable | not reachable |")
                rows.append(dict(experiment="E1", mu=mu, p=p, q_name=qname, reachable=0))
                continue
            md.append(f"| {mu:.2f} | {p:.3f} | {qname} | {b['regions']} | {b['spares']} | {b['yield_']:.3f} | "
                      f"{b['per_good']:.2f} | {b['per_good'] * mu:.2f} |")
            rows.append(dict(experiment="E1", mu=mu, p=p, q_name=qname, reachable=1, **b, absolute=b["per_good"] * mu))
    md.append("")
    md.append("Search limits: 1,200 register-file spares and 60 ALU spares. A row at a limit (and the mu = 1.6 row with "
              "yield 0.256) shows the limit, not an optimum.\n")

    md.append("## E2. Chip level: ALU width and organisation against mu\n")
    md.append("Register file fixed to the E1 optimum at routing q = 0.9992^mu; ALU spares chosen to minimise silicon per "
              "good chip; silicon x cycles with the M2 beat model, base = 2; normalised to the best K in each row group.\n")
    wls = rds.workloads()
    prog = sorted({p for p, _ in wls})[0]
    md.append("| mu | organisation | K* (silicon x cycles) | per good chip, own bare file = 1 at K* | ALU spares at K* |")
    md.append("|---:|:--|---:|---:|---:|")
    for mu in MUS:
        p = P0 ** mu
        q = Q_DEV ** mu
        rf = best_rf(p, q)
        if rf is None:
            md.append(f"| {mu:.2f} | both | no register file reaches 1e-6 yield | | |")
            continue
        for org in ("replicated", "single"):
            tab = []
            for k in KS:
                best = None
                for s in range(0, 61):
                    ya = rm.alu_yield(p, q, k, s, org)
                    if ya < 1e-6:
                        continue
                    a = cost.alu_total_T(k, s, org, Costs())
                    g = (rf["silicon_rel"] * BASE + a) / (rf["yield_"] * ya)
                    if best is None or g < best[0]:
                        best = (g, s, ya)
                if best:
                    x = best[0] * rds.cpi(wls[(prog, 2)], 2, k)
                    tab.append((x, k, best[0], best[1]))
            x, k, g, s = min(tab)
            md.append(f"| {mu:.2f} | {org} | {k} | {g / BASE:.2f} | {s} |")
            rows.append(dict(experiment="E2", mu=mu, org=org, k_star=k, per_good=g / BASE, alu_spares=s))
    md.append("")
    cols = sorted({k for r in rows for k in r})
    with open(out / "logic_style_study.csv", "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=cols)
        w.writeheader()
        w.writerows(rows)
    (out / "logic_style_study.md").write_text("\n".join(md) + "\n", encoding="utf-8")
    print("written:", out / "logic_style_study.csv", out / "logic_style_study.md")


if __name__ == "__main__":
    main()
