"""G9: do the yield conclusions survive clustered defects?

Simulation and exact computation only; every parameter is tier C except p (see config/params.toml).
The marginal slice yield stays exactly p = 0.71, so any change is due to correlation alone.

What it does
  1. what the published wafer map allows (matrix-scale dispersion bound, neighbour autocorrelation);
  2. validates the C++ simulator (real self-test and remap) against the exact model, point by point;
  3. computes the experiments with the exact model:
       E1 spares needed for 90% and 99% yield vs clustering strength and scale
       E2 yield vs how many regions the spares are split into (down to private per-register spares)
       E3 ALU spare area vs K under clustering
       E4 the TMR yield penalty when the register-file copies share a die
  4. writes results/cluster_study.csv, cluster_study.md and cluster_study.svg.

Usage (repo root, after building radslice_yield):   python scripts/cluster_study.py [--trials N] [--out DIR]
"""
import argparse
import csv
import math
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python"))
from radslice import cluster_model as cm  # noqa: E402
from radslice import wafer_map as wm  # noqa: E402
from radslice import yield_model as ym  # noqa: E402

EMU = next((p for p in (ROOT / "build/radslice_yield", ROOT / "build/radslice_yield.exe") if p.exists()), None)
P = 0.71
SEED = 7


def label(a):
    return "independent" if a is None else f"{a:g}"


def run_sim(trials, seed, spares, alpha=None, block=0, domains=1, copies=1, share_die=False):
    args = [str(EMU), "--trials", str(trials), "--seed", str(seed), "--p", str(P), "--k", "16",
            "--rf", str(spares), "--alu", "0", "--domains", str(domains), "--copies", str(copies)]
    if alpha is not None:
        args += ["--cluster-alpha", repr(alpha), "--cluster-block", str(block)]
    if share_die:
        args.append("--share-die")
    out = subprocess.run(args, capture_output=True, text=True, check=True).stdout
    row = next(csv.DictReader(out.splitlines()))
    return int(row["rf_ok"]) / int(row["trials"])


def spares_needed(target, alpha, block=None, smax=640):
    lo, hi = 0, smax
    if cm.pooled_yield(256, hi, P, alpha, block) < target:
        return None
    while lo < hi:                       # yield is monotone in the number of spares
        mid = (lo + hi) // 2
        if cm.pooled_yield(256, mid, P, alpha, block) >= target:
            hi = mid
        else:
            lo = mid + 1
    return lo


def alu_spares_needed(k, target, alpha, smax=200):
    for s in range(smax + 1):
        if cm.die_level_chip_yield(0, 0, k, s, P, alpha, True) >= target:
            return s
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--trials", type=int, default=20000)
    ap.add_argument("--out", default=str(ROOT / "results"))
    args = ap.parse_args()
    if EMU is None:
        sys.exit("build radslice_yield first (cmake --build build)")
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    rows, md = [], []

    def add(experiment, **kw):
        rows.append(dict(experiment=experiment, **kw))

    # ---- 1. what the wafer map allows -------------------------------------------------------------
    grid = wm.load_grid()
    counts = [r.count("R") for r in grid]
    p_map = 1 - sum(counts) / (8 * len(counts))
    a_min = cm.alpha_lower_bound(counts, 8, p_map)
    rho_max = cm.correlation(p_map, a_min)
    spatial = wm.spatial_autocorrelation(grid)
    md += ["## What the published wafer map allows", "",
           f"- Matrix scale (8 registers, matrices independent of each other): clustering stronger than "
           f"alpha = {a_min:.1f} (intra-block failure correlation {rho_max:.3f}) is rejected at about 95%.",
           f"- Neighbouring matrices: Moran's I = {spatial['morans_i']:.3f} (expected {spatial['expected_i']:.3f}), "
           f"P(positive autocorrelation) = {spatial['p_positive']:.2f}; neighbour correlation "
           f"{spatial['neighbour_correlation']:.2f}. No spatial structure between neighbours.",
           "- **Not constrained:** a defect rate common to every matrix on the wafer (wafer-, batch- or "
           "die-level variation) shifts all matrices together and leaves no difference between them, so one "
           "wafer cannot show it.", ""]
    add("data", metric="alpha_min_matrix_scale", value=a_min)
    add("data", metric="rho_max_matrix_scale", value=rho_max)
    add("data", metric="morans_i", value=spatial["morans_i"])
    add("data", metric="p_positive_autocorrelation", value=spatial["p_positive"])

    # ---- 2. validate the simulator against the exact model ----------------------------------------------
    n = args.trials
    cases = []                                    # (name, sim kwargs, exact value)
    for a in (20.0, a_min, 4.0, 1.0):
        for s in (128, 160):
            cases.append((f"pooled, die-level, alpha={a:.3g}, S={s}", dict(spares=s, alpha=a, block=0),
                          cm.pooled_yield(256, s, P, a)))
    for a in (a_min, 4.0, 1.0):
        for s in (128, 144):
            cases.append((f"pooled, blocks of 32, alpha={a:.3g}, S={s}", dict(spares=s, alpha=a, block=32),
                          cm.pooled_yield(256, s, P, a, block=32)))
    cases.append(("pooled, blocks of 8, alpha=4, S=128", dict(spares=128, alpha=4.0, block=8),
                  cm.pooled_yield(256, 128, P, 4.0, block=8)))
    for regions, s in ((8, 128), (32, 128)):
        d = 256 // regions + s // regions
        cases.append((f"{regions} regions, independent, S={s}", dict(spares=s, domains=regions),
                      cm.domain_yield(regions, 256 // regions, s // regions, P, None)))
        cases.append((f"{regions} regions, alpha=4, block = region, S={s}",
                      dict(spares=s, domains=regions, alpha=4.0, block=d),
                      cm.domain_yield(regions, 256 // regions, s // regions, P, 4.0, d)))
        cases.append((f"{regions} regions, alpha=4, die-level, S={s}", dict(spares=s, domains=regions, alpha=4.0, block=0),
                      cm.domain_yield(regions, 256 // regions, s // regions, P, 4.0, None)))
    for a in (20.0, 4.0):
        cases.append((f"3 register-file copies on one die, alpha={a:g}, S=144",
                      dict(spares=144, alpha=a, block=0, copies=3, share_die=True),
                      cm.die_level_chip_yield(3, 144, 16, 0, P, a, False)))
    md += ["## Simulator (real self-test and remap) against the exact model", "",
           f"{n} simulated chips per row; the exact column is computed independently by numerical integration.", "",
           "| case | simulated | exact | z |", "|---|---:|---:|---:|"]
    worst = 0.0
    for i, (name, kw, exact) in enumerate(cases):
        est = run_sim(n, SEED + i, **kw)
        sigma = math.sqrt(max(exact * (1 - exact), 1e-4) / n)
        z = (est - exact) / sigma
        worst = max(worst, abs(z))
        md.append(f"| {name} | {est:.4f} | {exact:.4f} | {z:+.2f} |")
        add("validation", case=name, simulated=est, exact=exact, z=z)
    md += ["", f"Largest |z| over {len(cases)} cases: {worst:.2f}.", ""]
    print(f"validation: {len(cases)} cases, largest |z| = {worst:.2f}")

    # ---- 3. E1: spares needed ---------------------------------------------------------------------------
    alphas = [None, 50.0, 20.0, a_min, 4.0, 1.0]
    md += ["## E1. Spares needed for a yield target (256 primary slices, p = 0.71, ideal shared pool)", "",
           "`alpha` is the clustering strength (smaller = stronger); `rho` the pairwise failure correlation inside a "
           "block. The data allow alpha of about "
           f"{a_min:.1f} or more at matrix scale. 'n/a' means the target cannot be reached with 640 spares.", "",
           "| alpha | rho | 90%, blocks of 32 | 99%, blocks of 32 | 90%, die-level | 99%, die-level | 99% area, die-level |",
           "|---|---:|---:|---:|---:|---:|---:|"]
    for a in alphas:
        r = 0.0 if a is None else cm.correlation(P, a)
        v = {t: (spares_needed(t, a, 32), spares_needed(t, a, None)) for t in (0.90, 0.99)}
        cell = lambda x: "n/a" if x is None else str(x)
        area = "n/a" if v[0.99][1] is None else f"{(256 + v[0.99][1]) / 256:.2f}x"
        md.append(f"| {label(a)} | {r:.3f} | {cell(v[0.90][0])} | {cell(v[0.99][0])} | {cell(v[0.90][1])} | "
                  f"{cell(v[0.99][1])} | {area} |")
        add("E1", alpha=label(a), rho=r, s90_block32=v[0.90][0], s99_block32=v[0.99][0],
            s90_die=v[0.90][1], s99_die=v[0.99][1])
    md += ["", "Yield at fixed spares:", "", "| spares | " + " | ".join(f"die-level, {label(a)}" for a in alphas) + " |",
           "|---:|" + "---:|" * len(alphas)]
    for s in (96, 112, 128, 144, 160, 192):
        ys = [cm.pooled_yield(256, s, P, a) for a in alphas]
        md.append(f"| {s} | " + " | ".join(f"{y:.3f}" for y in ys) + " |")
        for a, y in zip(alphas, ys):
            add("E1_curve", scale="die", alpha=label(a), spares=s, yield_=y)
        for a in alphas:
            add("E1_curve", scale="block32", alpha=label(a), spares=s, yield_=cm.pooled_yield(256, s, P, a, block=32))
    md.append("")

    # ---- 4. E2: restricted spare regions -----------------------------------------------------------------
    md += ["## E2. Spares restricted to regions (128 spares in total; 256 primaries)", "",
           "A region has its own primaries and its own spares, and a spare can serve only its own region. "
           "One region is the ideal pool; 32 regions is one region per register (4 private spares each), which "
           "is the M0 'per-register' case.", "",
           "| regions | independent | alpha = " + f"{a_min:.1f}" + ", block = region | alpha = 4, block = region | alpha = 4, die-level |",
           "|---:|---:|---:|---:|---:|"]
    for regions in (1, 2, 4, 8, 16, 32):
        s = 128
        if regions == 1:
            vals = [cm.pooled_yield(256, s, P, None), cm.pooled_yield(256, s, P, a_min, block=384),
                    cm.pooled_yield(256, s, P, 4.0, block=384), cm.pooled_yield(256, s, P, 4.0)]
        else:
            d = 256 // regions + s // regions
            D, sd = 256 // regions, s // regions
            vals = [cm.domain_yield(regions, D, sd, P, None), cm.domain_yield(regions, D, sd, P, a_min, d),
                    cm.domain_yield(regions, D, sd, P, 4.0, d), cm.domain_yield(regions, D, sd, P, 4.0, None)]
        md.append(f"| {regions} | " + " | ".join(f"{v:.4f}" for v in vals) + " |")
        add("E2", regions=regions, indep=vals[0], amin_block=vals[1], a4_block=vals[2], a4_die=vals[3])
    md.append("")

    # ---- 5. E3: ALU ----------------------------------------------------------------------------------------
    md += ["## E3. ALU spare area for 99% ALU yield, by slice width K (ALU treated as one clustered block)", "",
           "| alpha | K = 8 | K = 16 | K = 32 | ordering holds? |", "|---|---:|---:|---:|---|"]
    ok_all = True
    for a in (None, 20.0, a_min, 4.0):
        areas = []
        for k in (8, 16, 32):
            s = alu_spares_needed(k, 0.99, a)
            areas.append(None if s is None else (64 // k + s) / (64 // k))
        holds = all(x is not None for x in areas) and areas[0] <= areas[1] <= areas[2]
        ok_all = ok_all and holds
        cells = ["n/a" if x is None else f"{x:.2f}x" for x in areas]
        md.append(f"| {label(a)} | {cells[0]} | {cells[1]} | {cells[2]} | {'yes' if holds else 'NO'} |")
        add("E3", alpha=label(a), area_k8=areas[0], area_k16=areas[1], area_k32=areas[2], ordering_holds=holds)
    md.append("")

    # ---- 6. E4: TMR copies sharing a die -------------------------------------------------------------------------
    md += ["## E4. Register-file copies (TMR) when the copies share one die-level defect rate", "",
           "Boot yield of the register files only, per-copy spares as shown; alpha 'independent' means the "
           "copies fail independently.", "",
           "| spares per copy | alpha | 1 copy | 3 copies | 3 copies / 1 copy |", "|---:|---|---:|---:|---:|"]
    for s in (144, 160):
        for a in alphas:
            one = cm.die_level_chip_yield(1, s, 16, 0, P, a, False)
            three = cm.die_level_chip_yield(3, s, 16, 0, P, a, False)
            md.append(f"| {s} | {label(a)} | {one:.4f} | {three:.4f} | {three / one:.3f} |")
            add("E4", spares=s, alpha=label(a), one=one, three=three, ratio=three / one)
    md.append("")

    with open(out / "cluster_study.csv", "w", newline="") as f:
        names = sorted({k for r in rows for k in r})
        w = csv.DictWriter(f, fieldnames=names)
        w.writeheader()
        w.writerows(rows)
    (out / "cluster_study.md").write_text("\n".join(md))
    write_svg(out / "cluster_study.svg", alphas)
    print("written:", out / "cluster_study.csv", out / "cluster_study.md", out / "cluster_study.svg")
    sys.exit(1 if worst > 4.0 else 0)


def write_svg(path, alphas):
    W, H, M = 780, 330, 48
    colors = ["#222222", "#1f6feb", "#0f9d58", "#d97706", "#c62828", "#7c3aed"]

    def panel(x0, title, block):
        pw, ph = 320, H - 2 * M
        g = [f'<text x="{x0}" y="22" font-size="14" font-weight="600">{title}</text>']
        for t in range(0, 11, 2):
            y = M + ph - ph * t / 10
            g.append(f'<line x1="{x0}" x2="{x0 + pw}" y1="{y}" y2="{y}" stroke="#ddd"/>'
                     f'<text x="{x0 - 6}" y="{y + 4}" font-size="10" text-anchor="end">{t / 10:.1f}</text>')
        for t in range(80, 261, 40):
            g.append(f'<text x="{x0 + pw * (t - 80) / 180}" y="{M + ph + 16}" font-size="10" text-anchor="middle">{t}</text>')
        g.append(f'<text x="{x0 + pw / 2}" y="{H - 8}" font-size="11" text-anchor="middle">spare slices (of 256)</text>')
        for i, a in enumerate(alphas):
            pts = [(s, cm.pooled_yield(256, s, P, a, block)) for s in range(80, 261, 4)]
            d = " ".join(f"{'M' if j == 0 else 'L'}{x0 + pw * (s - 80) / 180:.1f},{M + ph - ph * y:.1f}"
                         for j, (s, y) in enumerate(pts))
            g.append(f'<path d="{d}" fill="none" stroke="{colors[i]}" stroke-width="1.6"/>')
            g.append(f'<text x="{x0 + pw + 4}" y="{M + 12 + 13 * i}" font-size="10" fill="{colors[i]}">'
                     f'{"indep." if a is None else "a=" + format(a, ".3g")}</text>')
        return "\n".join(g)

    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" font-family="system-ui, sans-serif">'
           f'<rect width="{W}" height="{H}" fill="white"/><g fill="#222">'
           + panel(40, "Clustering in blocks of 32 slices", 32)
           + panel(420, "Die-level clustering (one rate for the whole file)", None)
           + f'</g><text x="{W - 6}" y="{H - 6}" font-size="9" text-anchor="end" fill="#666">'
             'exact model, p = 0.71 fixed; alpha: smaller = stronger clustering. Hypothetical, not a measurement.</text></svg>')
    path.write_text(svg)


if __name__ == "__main__":
    main()
