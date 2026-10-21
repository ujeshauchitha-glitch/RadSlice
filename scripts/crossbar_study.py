"""G10: what does the spare-routing crossbar cost, and does sparing still pay?

Exact models only (no simulation): the yield models of G9 plus the transistor-count cost model in
python/radslice/cost_model.py. Marginal slice yield is p = 0.71 (tier B); every cost knob is tier C.

  E1  crossbar vs storage: routing silicon as a multiple of the slices it protects
  E2  cheapest register file per region count, and cheapest design reaching 90% / 99% yield
  E3  chip-level cost (register file + ALU, joint optimum) vs K, for both ALU organisations,
      and silicon x cycles using the M2 beat model
  E4  sensitivity of E2/E3 conclusions to every tier-C cost knob

Usage (repo root):   python scripts/crossbar_study.py [--out DIR]
"""
import argparse
import csv
import itertools
import math
import sys
from functools import lru_cache
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python"))
from radslice import cluster_model as cm  # noqa: E402
from radslice import cost_model as cost  # noqa: E402
from radslice.cost_model import Costs  # noqa: E402
from radslice.params import load_params  # noqa: E402

P = load_params()["yield.p_slice_good"].value
ALPHA_DIE = 9.4                     # strongest die-level clustering the wafer map allows (rounded)
REGIONS = (1, 2, 4, 8, 16, 32, 64)
KS = (1, 2, 4, 8, 16, 32, 64)     # 64 = the unsliced datapath, for reference
ORGS = ("replicated", "single")
BASE = cost.RF_PRIMARIES * cost.T_RF_BLOCK


# ---- yields (cached) -----------------------------------------------------------------------

@lru_cache(maxsize=None)
def rf_yield(regions, spares, alpha=None):
    if regions == 1:
        return cm.pooled_yield(cost.RF_PRIMARIES, spares, P, alpha)
    d = cost.RF_PRIMARIES // regions
    return cm.domain_yield(regions, d, spares // regions, P, alpha)


@lru_cache(maxsize=None)
def alu_yield(k, spares, org):
    pos = 1 if org == "single" else None
    return cm.die_level_chip_yield(0, 0, k, spares, P, None, True, alu_positions=pos)


def rf_grid(regions, alpha=None):
    """Spare counts worth trying for this region count (multiples of the region count)."""
    top = 420 if alpha is None else 360
    step = 1 if alpha is None else 2
    return [regions * s for s in range(0, top // regions + 1, step)]


def rf_designs(c, alpha=None, regions=REGIONS):
    out = []
    for n in regions:
        for s in rf_grid(n, alpha):
            y = rf_yield(n, s, alpha)
            if y > 1e-9:
                out.append((n, s, cost.rf_total_T(s, n, c), y))
    return out


def best_rf(c, alpha=None, regions=REGIONS, target=0.0):
    ds = [d for d in rf_designs(c, alpha, regions) if d[3] >= target]
    return min(ds, key=lambda d: d[2] / d[3]) if ds else None


# ---- M2 beat model -------------------------------------------------------------------------

def workloads():
    """cycles per instruction at slice width K, from results/m2_cycles.csv (beats scale as 1/K)."""
    rows = {}
    with open(ROOT / "results" / "m2_cycles.csv", newline="") as f:
        for r in csv.DictReader(f):
            if int(r["K"]) == 8:
                rows[(r["program"], int(r["base_cycles"]))] = (
                    int(r["instret"]), int(r["beats"]), int(r["alu_ops"]))
    return rows


def cpi(wl, base, k):
    instret, beats8, ops = wl
    return base + max(beats8 * 8.0 / k, ops) / instret      # every ALU op takes at least one beat


# ---- chip-level joint optimum --------------------------------------------------------------

def best_chip(k, org, c, alpha=None, regions=REGIONS):
    """Joint choice of register-file design and ALU spares minimising silicon per working chip."""
    rfs = rf_designs(c, alpha, regions)
    best = None
    for s in range(0, 61):
        ya = alu_yield(k, s, org)
        if ya < 1e-9:
            continue
        a = cost.alu_total_T(k, s, org, c)
        for n, rs, r, yr in rfs:
            cg = (r + a + c.other_T) / (yr * ya)
            if best is None or cg < best["per_good"]:
                best = dict(k=k, org=org, regions=n, rf_spares=rs, alu_spares=s, rf_T=r, alu_T=a,
                            yield_rf=yr, yield_alu=ya, per_good=cg)
    return best


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "results"))
    out = Path(ap.parse_args().out)
    out.mkdir(parents=True, exist_ok=True)
    rows, md = [], []

    def add(experiment, **kw):
        rows.append(dict(experiment=experiment, **kw))

    c0 = Costs()
    md.append("# G10: the cost of the spare-routing crossbar\n")
    md.append(f"Exact models; p = {P} (tier B); every cost knob is tier C. Transistor count stands in for area, "
              "wiring and layout are ignored, so routing costs are more likely too low than too high. "
              "Default knobs: tagged spare array, 2 read ports, 1 write port, 9 T per tag bit per port for "
              "compare, ALU unit = 27 T per bit, nothing outside register file and ALU.\n")

    # E1
    md.append("## E1. Routing silicon against the slices it protects\n")
    md.append("Ideal pool (1 region). Silicon in units of the 256-slice bare register file "
              f"({BASE:,.0f} T).\n")
    md.append("| spares | spare storage | tagged routing | per-position routing |")
    md.append("|---:|---:|---:|---:|")
    for s in (32, 64, 96, 128, 192, 256):
        st = s * cost.T_RF_BLOCK / BASE
        tg = cost.rf_routing_T(s, 1, Costs(topology="tagged")) / BASE
        pp = cost.rf_routing_T(s, 1, Costs(topology="per_position")) / BASE
        md.append(f"| {s} | {st:.2f} | {tg:.2f} | {pp:.2f} |")
        add("E1", spares=s, storage_rel=st, tagged_rel=tg, per_position_rel=pp)
    md.append("")

    # E2
    md.append("## E2. Cheapest register file per region count\n")
    md.append(f"Silicon per working register file, relative to the bare 256-slice file. "
              f"No sparing at all yields {P ** 256:.1e}, so it is not a candidate.\n")
    md.append("Reference designs (independent defects, tagged routing, default costs):\n")
    md.append("| design | silicon rel. | yield | cost per good rel. | same, crossbar free |")
    md.append("|:--|---:|---:|---:|---:|")
    for name, n, s in (("C2 design: 128 spares, ideal pool", 1, 128), ("160 spares, ideal pool", 1, 160)):
        y = rf_yield(n, s)
        tot = cost.rf_total_T(s, n, c0)
        md.append(f"| {name} | {tot / BASE:.2f} | {y:.3f} | {tot / y / BASE:.2f} | {cost.rf_storage_T(s) / y / BASE:.2f} |")
        add("E2_ref", design=name, silicon_rel=tot / BASE, yield_=y, per_good_rel=tot / y / BASE)
    md.append("")
    md.append("The search stops at 420 spares (360 under clustering), so the 64-region rows are limited by that cap "
              "and are not minima.\n")
    for label, alpha in (("independent defects", None), (f"die-level clustering, alpha = {ALPHA_DIE}", ALPHA_DIE)):
        md.append(f"### {label}\n")
        md.append("| regions | topology | best spares | yield | silicon rel. | cost per good rel. | same, crossbar free |")
        md.append("|---:|:--|---:|---:|---:|---:|---:|")
        for n in REGIONS:
            free = best_rf(Costs(routing_scale=0.0),
                           alpha, (n,))
            for topo in ("tagged", "per_position"):
                cc = Costs(topology=topo)
                b = best_rf(cc, alpha, (n,))
                if b is None:
                    continue
                md.append(f"| {n} | {topo} | {b[1]} | {b[3]:.3f} | {b[2] / BASE:.2f} | {b[2] / b[3] / BASE:.2f} | "
                          f"{free[2] / free[3] / BASE:.2f} (S={free[1]}) |")
                add("E2", alpha=alpha or "", regions=n, topology=topo, spares=b[1], yield_=b[3],
                    silicon_rel=b[2] / BASE, per_good_rel=b[2] / b[3] / BASE,
                    free_per_good_rel=free[2] / free[3] / BASE, free_spares=free[1])
        md.append("")
        md.append("Cheapest design that reaches a yield target (tagged routing):\n")
        md.append("| target | regions | spares | yield | silicon rel. | cost per good rel. |")
        md.append("|---:|---:|---:|---:|---:|---:|")
        for t in (0.9, 0.99):
            b = best_rf(c0, alpha, REGIONS, t)
            if b:
                md.append(f"| {t:.0%} | {b[0]} | {b[1]} | {b[3]:.3f} | {b[2] / BASE:.2f} | {b[2] / b[3] / BASE:.2f} |")
                add("E2_target", alpha=alpha or "", target=t, regions=b[0], spares=b[1], yield_=b[3],
                    silicon_rel=b[2] / BASE)
            else:
                md.append(f"| {t:.0%} | not reachable in the spares range searched | | | | |")
        md.append("")

    # E3
    md.append("## E3. Chip cost against slice width K\n")
    md.append("Register file and ALU optimised jointly (independent defects, default costs). Silicon per good chip "
              "in units of the bare register file; cycles per instruction from the M2 beat model with the "
              "outside-the-ALU cost `base` = 2 beats; silicon x cycles normalised to the best row in the table.\n")
    wls = workloads()
    progs = sorted({p for p, _ in wls})
    for org in ORGS:
        md.append(f"### ALU organisation: {org}\n")
        md.append("| K | RF regions | RF spares | ALU spares | ALU yield | ALU silicon (T) | per good chip rel. | "
                  + " | ".join(f"x cycles: {p[:18]}" for p in progs) + " |")
        md.append("|---:|---:|---:|---:|---:|---:|---:|" + "---:|" * len(progs))
        table = [best_chip(k, org, c0) for k in KS]
        norms = {p: min(r["per_good"] * cpi(wls[(p, 2)], 2, r["k"]) for r in table) for p in progs}
        for r in table:
            xs = [r["per_good"] * cpi(wls[(p, 2)], 2, r["k"]) / norms[p] for p in progs]
            md.append(f"| {r['k']} | {r['regions']} | {r['rf_spares']} | {r['alu_spares']} | {r['yield_alu']:.3f} | "
                      f"{r['alu_T']:,.0f} | {r['per_good'] / BASE:.3f} | " + " | ".join(f"{x:.2f}" for x in xs) + " |")
            add("E3", org=org, k=r["k"], regions=r["regions"], rf_spares=r["rf_spares"], alu_spares=r["alu_spares"],
                yield_alu=r["yield_alu"], alu_T=r["alu_T"], per_good_rel=r["per_good"] / BASE,
                **{f"x_cycles[{p}]": x for p, x in zip(progs, xs)})
        md.append("")

    # E4
    md.append("## E4. Which cost assumption changes which answer\n")
    md.append("One knob moved at a time from the default (independent defects). `K*` is the K with the lowest "
              "silicon x cycles for the first workload at base = 2; `ALU/chip` is the ALU's share of chip silicon "
              "at the K* design.\n")
    md.append("| knob | value | RF regions | RF spares | RF cost per good rel. | K* replicated | K* single | ALU/chip (single K*) |")
    md.append("|:--|:--|---:|---:|---:|---:|---:|---:|")
    knobs = [("default", "", {})]
    knobs += [("topology", "per_position", dict(topology="per_position"))]
    knobs += [("read_ports", v, dict(read_ports=v)) for v in (1, 3)]
    knobs += [("tag_compare_T", v, dict(tag_compare_T=v)) for v in (0.0, 27.0)]
    knobs += [("t_mux2 x", v, dict(t_mux2=cost.T_MUX2 * v)) for v in (0.5, 2.0, 4.0)]
    knobs += [("alu_factor", v, dict(alu_factor=v)) for v in (4.0, 16.0)]
    knobs += [("other_T", v, dict(other_T=v)) for v in (6000.0, 60000.0)]
    knobs += [("base_cycles", v, dict(base=v)) for v in (1, 4)]
    for name, val, kw in knobs:
        base = kw.pop("base", 2)
        cc = Costs(**kw)
        b = best_rf(cc)
        kstar = {}
        keep = None
        for org in ORGS:
            tab = [best_chip(k, org, cc) for k in KS]
            w = min(tab, key=lambda r: r["per_good"] * cpi(wls[(progs[0], base)], base, r["k"]))
            kstar[org] = w["k"]
            if org == "single":
                keep = w
        share = keep["alu_T"] / (keep["rf_T"] + keep["alu_T"] + cc.other_T)
        md.append(f"| {name} | {val} | {b[0]} | {b[1]} | {b[2] / b[3] / BASE:.2f} | {kstar['replicated']} | "
                  f"{kstar['single']} | {share:.3f} |")
        add("E4", knob=name, value=val, regions=b[0], rf_spares=b[1], rf_per_good_rel=b[2] / b[3] / BASE,
            k_star_replicated=kstar["replicated"], k_star_single=kstar["single"], alu_share=share)
    md.append("")

    cols = sorted({k for r in rows for k in r})
    with open(out / "crossbar_study.csv", "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=cols)
        w.writeheader()
        w.writerows(rows)
    (out / "crossbar_study.md").write_text("\n".join(md) + "\n", encoding="utf-8")
    write_svg(out / "crossbar_study.svg", wls, progs[0])
    print("written:", out / "crossbar_study.csv", out / "crossbar_study.md", out / "crossbar_study.svg")


def write_svg(path, wls, prog):
    W, H, M = 780, 330, 48
    ph, pw = H - 2 * M, 300

    def panel(x0, title, series, xlabels, ylo, yhi, ylab):
        g = [f'<text x="{x0}" y="22" font-size="14" font-weight="600">{title}</text>']
        lo, hi = math.log10(ylo), math.log10(yhi)
        for e in range(math.ceil(lo), math.floor(hi) + 1):
            y = M + ph - ph * (e - lo) / (hi - lo)
            g.append(f'<line x1="{x0}" x2="{x0 + pw}" y1="{y}" y2="{y}" stroke="#ddd"/>'
                     f'<text x="{x0 - 6}" y="{y + 4}" font-size="10" text-anchor="end">{10.0 ** e:g}</text>')
        for i, xl in enumerate(xlabels):
            g.append(f'<text x="{x0 + pw * i / (len(xlabels) - 1)}" y="{M + ph + 16}" font-size="10" '
                     f'text-anchor="middle">{xl}</text>')
        g.append(f'<text x="{x0 + pw / 2}" y="{H - 8}" font-size="11" text-anchor="middle">{ylab}</text>')
        for j, (name, col, ys) in enumerate(series):
            pts = [(x0 + pw * i / (len(ys) - 1), M + ph - ph * (math.log10(max(y, ylo)) - lo) / (hi - lo))
                   for i, y in enumerate(ys)]
            d = " ".join(f"{'M' if i == 0 else 'L'}{x:.1f},{y:.1f}" for i, (x, y) in enumerate(pts))
            g.append(f'<path d="{d}" fill="none" stroke="{col}" stroke-width="1.8"/>')
            g.append(f'<text x="{x0 + 6}" y="{M + 12 + 13 * j}" font-size="10" fill="{col}">{name}</text>')
        return "\n".join(g)

    free = Costs(routing_scale=0.0)
    s1 = []
    for topo, col, c in (("crossbar free", "#0f9d58", free), ("tagged", "#1f6feb", Costs()),
                         ("per-position", "#c62828", Costs(topology="per_position"))):
        ys = []
        for n in REGIONS:
            b = best_rf(c, None, (n,))
            ys.append(b[2] / b[3] / BASE)
        s1.append((topo, col, ys))
    s2 = []
    for org, col in (("replicated ALU", "#d97706"), ("single-unit ALU", "#7c3aed")):
        ys = []
        for k in KS:
            r = best_chip(k, "replicated" if org.startswith("rep") else "single", Costs())
            ys.append(r["per_good"] * cpi(wls[(prog, 2)], 2, k) / BASE)
        s2.append((org, col, ys))
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" font-family="system-ui, sans-serif">'
           f'<rect width="{W}" height="{H}" fill="white"/><g fill="#222">'
           + panel(60, "Register file: silicon per good file", s1, [str(n) for n in REGIONS], 1, 100,
                   "regions the spares are split into (relative to bare file)")
           + panel(430, "Chip: silicon per good chip x cycles/instr", s2, [str(k) for k in KS], 1, 1000,
                   "slice width K (bits)")
           + f'</g><text x="{W - 6}" y="{H - 6}" font-size="9" text-anchor="end" fill="#666">'
             'exact models, p = 0.71, independent defects; transistor-count area proxy, tier-C costs. Not a measurement.</text></svg>')
    path.write_text(svg, encoding="utf-8")


if __name__ == "__main__":
    main()
