"""G14: do the spares and routing survive their own defects?

Exact models. Each spare carries its private routing (a fault in it disables that spare); a shared block
of 1,152 transistors is fatal unless replicated. p = 0.71 (tier B); q, the per-transistor yield, is
swept: 0.99762 is implied by the register measurements (tier B), 0.9992 is the isolated-device figure
(tier A), and q = 1 reproduces G10.

  E1  the cheapest register file against q: design, yield, cost per good file
  E2  what happens to one spare: its yield against q at the G10 optimum
  E3  chip level: ALU spares with their routing, and the K ordering again

Usage (repo root):   python scripts/routing_defect_study.py [--out DIR]
"""
import argparse
import csv
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python"))
from radslice import cost_model as cost  # noqa: E402
from radslice import routing_model as rm  # noqa: E402
from radslice.cost_model import Costs  # noqa: E402
from radslice.params import load_params  # noqa: E402

PRM = load_params()
P = PRM["yield.p_slice_good"].value
Q_REG = PRM["yield.implied_per_transistor"].value
Q_DEV = PRM["yield.measured_transistor"].value
QS = (1.0, 0.99999, 0.9999, 0.9996, Q_DEV, 0.9985, Q_REG)
REGIONS = (1, 2, 4, 8, 16, 32)
COPIES = (1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64)
BASE = cost.RF_PRIMARIES * cost.T_RF_BLOCK
KS = (1, 2, 4, 8, 16, 32)


def best_rf(q, topology="tagged", target=0.0, shared_T=rm.SHARED_T):
    c = Costs(topology=topology)
    best = None
    for n in REGIONS:
        top = 1200
        for s in range(n, top + 1, n if n > 1 else 1):
            core = rm.rf_core_yield(P, q, s, n, topology)
            for m in COPIES:
                if shared_T == 0 and m > 1:
                    continue
                y = core * rm.shared_yield(q, s, m, shared_T)
                if y < max(1e-12, target):
                    continue
                t = rm.rf_silicon_T(s, n, m, c, shared_T)
                g = t / y
                if best is None or g / BASE < best["per_good"]:
                    best = dict(q=q, topology=topology, shared_T=shared_T, regions=n, spares=s, copies=m, yield_=y, silicon_rel=t / BASE,
                                per_good=g / BASE)
    return best


def workloads():
    rows = {}
    with open(ROOT / "results" / "m2_cycles.csv", newline="") as f:
        for r in csv.DictReader(f):
            if int(r["K"]) == 8:
                rows[(r["program"], int(r["base_cycles"]))] = (int(r["instret"]), int(r["beats"]), int(r["alu_ops"]))
    return rows


def cpi(wl, base, k):
    instret, beats8, ops = wl
    return base + max(beats8 * 8.0 / k, ops) / instret


def best_chip(k, org, q, rf):
    """ALU spares chosen with their routing counted; register file fixed to the design `rf`."""
    c = Costs()
    best = None
    for s in range(0, 61):
        ya = rm.alu_yield(P, q, k, s, org)
        if ya < 1e-6:
            continue
        a = cost.alu_total_T(k, s, org, c)
        g = (rf["silicon_rel"] * BASE + a) / (rf["yield_"] * ya)
        if best is None or g < best["per_good"]:
            best = dict(k=k, org=org, alu_spares=s, yield_alu=ya, alu_T=a, per_good=g)
    return best


def fmt_q(q):
    return "1 (G10)" if q == 1.0 else f"{q:g}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "results"))
    out = Path(ap.parse_args().out)
    out.mkdir(parents=True, exist_ok=True)
    rows, md = [], []

    def add(experiment, **kw):
        rows.append(dict(experiment=experiment, **kw))

    md.append("# G14: do the spares and routing survive their own defects?\n")
    md.append(f"Exact models, independent defects, p = {P}. q is the per-transistor yield of the routing: "
              f"{Q_REG} is implied by the register measurements (tier B), {Q_DEV} is the isolated-device figure "
              "(tier A), 1 is the fault-free routing of G10. Each spare carries private routing that fails with it; "
              f"a shared block of {rm.SHARED_T:.0f} transistors is fatal unless replicated. Costs are relative to "
              "the bare 256-slice register file.\n")

    md.append("## E1. The cheapest register file against q\n")
    md.append("Rows marked * found no design above 50% yield within the search range (up to 1,200 spares, 64 shared "
              "copies): the cost shown is the best of poor options and is not a design.\n")
    md.append("| q | topology | shared block | regions | spares | shared copies | yield | silicon rel. | cost per good rel. | vs q = 1 |")
    md.append("|---:|:--|:--|---:|---:|---:|---:|---:|---:|---:|")
    ref = {}
    for topo, sh in (("tagged", 0.0), ("tagged", rm.SHARED_T), ("per_position", rm.SHARED_T)):
        for q in QS:
            b = best_rf(q, topo, shared_T=sh)
            if q == 1.0:
                ref[(topo, sh)] = b["per_good"]
            star = "" if b["yield_"] >= 0.5 else " *"
            md.append(f"| {fmt_q(q)} | {topo} | {'none' if sh == 0 else f'{sh:.0f} T'} | {b['regions']} | {b['spares']} | "
                      f"{b['copies']} | {b['yield_']:.3f} | {b['silicon_rel']:.2f} | {b['per_good']:.2f}{star} | "
                      f"{b['per_good'] / ref[(topo, sh)]:.2f}x |")
            add("E1", **b, vs_ideal=b["per_good"] / ref[(topo, sh)])
    md.append("")

    md.append("## E2. One spare at the G10 optimum (8 regions, 192 spares, tagged)\n")
    priv = rm.spare_private_T(192, 8)
    md.append(f"Each spare has {priv:.0f} private routing transistors, so its yield is {P} x q^{priv:.0f}.\n")
    md.append("| q | spare yield | primaries that must be covered (mean) | spares needed per region (mean) |")
    md.append("|---:|---:|---:|---:|")
    for q in QS:
        ps = P * q ** priv
        md.append(f"| {fmt_q(q)} | {ps:.3f} | {256 * (1 - P):.1f} | {256 * (1 - P) / 8 / ps:.1f} of 24 |")
        add("E2", q=q, spare_yield=ps)
    md.append("")

    md.append("## E3. Chip level: ALU spares with their routing, and K again\n")
    md.append("Register file fixed to the cheapest design found in E1 for that q (tagged); ALU spares chosen to "
              "minimise silicon per good chip; silicon x cycles uses the M2 beat model, base = 2, normalised to the "
              "best K in each row group.\n")
    wls = workloads()
    prog = sorted({p for p, _ in wls})[0]
    for q in (1.0, Q_DEV, Q_REG):
        rf = best_rf(q)
        md.append(f"### q = {fmt_q(q)}  (register file: {rf['regions']} regions, {rf['spares']} spares, "
                  f"{rf['copies']} shared copies, yield {rf['yield_']:.3f})\n")
        md.append("| org | K | ALU spares | ALU yield | ALU silicon (T) | per good chip rel. | silicon x cycles |")
        md.append("|:--|---:|---:|---:|---:|---:|---:|")
        for org in ("replicated", "single"):
            tab = [best_chip(k, org, q, rf) for k in KS]
            tab = [t for t in tab if t]
            norm = min(t["per_good"] * cpi(wls[(prog, 2)], 2, t["k"]) for t in tab)
            for t in tab:
                x = t["per_good"] * cpi(wls[(prog, 2)], 2, t["k"]) / norm
                md.append(f"| {org} | {t['k']} | {t['alu_spares']} | {t['yield_alu']:.3f} | {t['alu_T']:,.0f} | "
                          f"{t['per_good'] / BASE:.3f} | {x:.2f} |")
                add("E3", q=q, **t, x_cycles=x)
        md.append("")

    cols = sorted({k for r in rows for k in r})
    with open(out / "routing_defect_study.csv", "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=cols)
        w.writeheader()
        w.writerows(rows)
    (out / "routing_defect_study.md").write_text("\n".join(md) + "\n", encoding="utf-8")
    write_svg(out / "routing_defect_study.svg", rows)
    print("written:", out / "routing_defect_study.csv", out / "routing_defect_study.md")


def write_svg(path, rows):
    W, H, M, pw = 560, 330, 48, 440
    ph = H - 2 * M
    e1 = [r for r in rows if r["experiment"] == "E1"]
    series = [("tagged", 0.0, "#0f9d58", "tagged, spares' private routing only"),
              ("tagged", rm.SHARED_T, "#1f6feb", "tagged, plus shared block"),
              ("per_position", rm.SHARED_T, "#c62828", "per-position, plus shared block")]
    lo, hi = 0.0, 3.0
    e1 = [r for r in e1 if r["yield_"] >= 0.5]
    g = [f'<text x="40" y="22" font-size="14" font-weight="600">Register file: silicon per good file vs routing '
         'transistor yield</text>']
    for e in range(0, math.ceil(hi)):
        y = M + ph - ph * (e - lo) / (hi - lo)
        g.append(f'<line x1="40" x2="{40 + pw}" y1="{y}" y2="{y}" stroke="#ddd"/>'
                 f'<text x="34" y="{y + 4}" font-size="10" text-anchor="end">{10 ** e}</text>')
    for i, q in enumerate(QS):
        x = 40 + pw * i / (len(QS) - 1)
        g.append(f'<text x="{x}" y="{M + ph + 16}" font-size="9" text-anchor="middle">{q:g}</text>')
    g.append(f'<text x="{40 + pw / 2}" y="{H - 8}" font-size="11" text-anchor="middle">per-transistor yield q of the routing</text>')
    for j, (topo, sh, col, name) in enumerate(series):
        pts = []
        for i, q in enumerate(QS):
            r = next((r for r in e1 if r["topology"] == topo and r["q"] == q and r["shared_T"] == sh), None)
            if r is None:
                continue
            pts.append((40 + pw * i / (len(QS) - 1), M + ph - ph * (math.log10(r["per_good"]) - lo) / (hi - lo)))
        d = " ".join(f"{'M' if i == 0 else 'L'}{x:.1f},{y:.1f}" for i, (x, y) in enumerate(pts))
        g.append(f'<path d="{d}" fill="none" stroke="{col}" stroke-width="1.8"/>'
                 f'<text x="{48}" y="{M + 12 + 13 * j}" font-size="10" fill="{col}">{name}</text>')
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" font-family="system-ui, sans-serif">'
           f'<rect width="{W}" height="{H}" fill="white"/><g fill="#222">' + "\n".join(g) +
           f'</g><text x="{W - 6}" y="{H - 6}" font-size="9" text-anchor="end" fill="#666">'
           'exact model, p = 0.71, independent defects, tier-C costs. Not a measurement.</text></svg>')
    path.write_text(svg, encoding="utf-8")


if __name__ == "__main__":
    main()
