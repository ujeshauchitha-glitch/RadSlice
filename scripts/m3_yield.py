"""M3: yield versus spares from the chip simulator, checked against the closed form.

Simulation only; every defect parameter is tier C. For each configuration this runs
`radslice_yield` (real boot self-test and remap on the real register-file and ALU
models, over sampled defects), compares the simulated pass rate with the closed form
in python/radslice/yield_model.py, and fails if any point is more than 4 standard
errors away. Writes results/m3_yield.csv and results/m3_yield.svg.

Usage (repo root, after building):   python scripts/m3_yield.py [--trials N]
"""
import argparse
import csv
import math
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python"))
from radslice import yield_model as ym  # noqa: E402
from radslice.params import load_params  # noqa: E402

EMU = next((p for p in (ROOT / "build/radslice_yield", ROOT / "build/radslice_yield.exe") if p.exists()), None)
SEED = 1
RF_SPARES = list(range(0, 161, 16))
ALU_SPARES = {8: list(range(0, 11)), 16: list(range(0, 9)), 32: list(range(0, 7))}
RF_FIXED_FOR_ALU = 128   # register-file spares used while sweeping the ALU (for the chip product)


def run(args):
    out = subprocess.run([str(EMU), *args], capture_output=True, text=True, check=True).stdout
    return list(csv.DictReader(out.splitlines()))


def z_and_ok(hits, n, exact):
    est = hits / n
    sigma = math.sqrt(exact * (1 - exact) / n)
    tol = max(4 * sigma, 3.0 / n)       # near 0 or 1 sigma vanishes; allow a few events
    z = 0.0 if sigma == 0 else (est - exact) / sigma
    return est, sigma, z, abs(est - exact) <= tol


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--trials", type=int, default=20000)
    args = ap.parse_args()
    if EMU is None:
        sys.exit("build radslice_yield first (cmake --build build)")

    p = load_params()["yield.p_slice_good"]
    p8 = p.value
    n = args.trials
    rows, bad = [], []

    def record(kind, k, rf_s, alu_s, hits, exact):
        est, sigma, z, ok = z_and_ok(hits, n, exact)
        rows.append(dict(kind=kind, k=k, rf_spares=rf_s, alu_spares=alu_s, trials=n,
                         simulated=est, closed_form=exact, std_error=sigma, z=z, within_4_sigma=ok))
        if not ok:
            bad.append((kind, k, rf_s, alu_s, est, exact, z))

    # Register file: spares sweep.
    out = run(["--mode", "yield", "--trials", str(n), "--seed", str(SEED), "--p", str(p8), "--k", "16",
               "--rf", ",".join(map(str, RF_SPARES)), "--alu", "0"])
    for r in out:
        s = int(r["rf_spares"])
        record("regfile", 16, s, 0, int(r["rf_ok"]), ym.file_yield_shared_pool(256, s, p8))
        # Sanity: mean defective primaries should be 256 * (1 - p).
    mean_def = float(out[0]["mean_rf_defective_primary"])

    # ALU: spares sweep per K, with the register file fixed, so chip yield is also checked.
    for k, spares in ALU_SPARES.items():
        out = run(["--mode", "yield", "--trials", str(n), "--seed", str(SEED + k), "--p", str(p8),
                   "--k", str(k), "--rf", str(RF_FIXED_FOR_ALU), "--alu", ",".join(map(str, spares))])
        rf_exact = ym.file_yield_shared_pool(256, RF_FIXED_FOR_ALU, p8)
        for r in out:
            s = int(r["alu_spares"])
            a_exact = ym.alu_yield(k, s, p8)
            record("alu", k, RF_FIXED_FOR_ALU, s, int(r["alu_ok"]), a_exact)
            record("chip", k, RF_FIXED_FOR_ALU, s, int(r["both_ok"]), rf_exact * a_exact)

    outdir = ROOT / "results"
    outdir.mkdir(exist_ok=True)
    with open(outdir / "m3_yield.csv", "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0]))
        w.writeheader()
        w.writerows(rows)
    write_svg(rows, outdir / "m3_yield.svg", p8)

    zmax = max(abs(r["z"]) for r in rows)
    print(f"p_slice_good = {p8} (tier {p.tier}, hypothetical); {n} chips per point, seed {SEED}")
    print(f"mean defective primary register slices: {mean_def:.2f} (expected {256 * (1 - p8):.2f})")
    print(f"{len(rows)} points; largest |z| = {zmax:.2f}; outside 4 sigma: {len(bad)}")
    for b in bad:
        print("  OUTSIDE:", b)

    print("\nALU spare area needed for a target yield (closed form, ideal crossbar):")
    print(f"{'K':>3} {'target':>7} {'spares':>7} {'ALU area':>9}")
    for target in (0.9, 0.99):
        for k in (8, 16, 32):
            s = ym.min_spares_for(k, p8, target)
            print(f"{k:3d} {target:7.2f} {s:7d} {ym.alu_area_ratio(k, s):8.3f}x")
    print(f"\nwritten: {outdir / 'm3_yield.csv'}, {outdir / 'm3_yield.svg'}")

    weak_bad = weak_section(n, outdir)
    sys.exit(1 if (bad or weak_bad) else 0)


def weak_section(n, outdir):
    """Intermittent ('weak') defects: do they escape the self-test, and how fast do more
    test rounds close the gap? A weak defect shows with probability q on each access."""
    qs, rounds = [0.05, 0.1, 0.3, 0.5], [1, 2, 4, 8, 16]
    rows = run(["--mode", "weak", "--trials", str(n), "--seed", str(SEED + 100),
                "--q", ",".join(map(str, qs)), "--rounds", ",".join(map(str, rounds))])
    out, outside = [], 0
    print(f"\nWeak defects, one defective register slice, detection by R test rounds (closed form 1-(1-q)^(2R)):")
    print(f"{'q':>5} {'R':>3} {'detected':>9} {'closed form':>12} {'z':>6}")
    for r in rows:
        q, R, h = float(r["activation"]), int(r["rounds"]), int(r["detected"])
        exact = 1 - (1 - q) ** (2 * R)
        est, sigma, z, ok = z_and_ok(h, n, exact)
        outside += not ok
        out.append(dict(activation=q, rounds=R, trials=n, detected=est, closed_form=exact, z=z))
        print(f"{q:5.2f} {R:3d} {est:9.4f} {exact:12.4f} {z:6.2f}")
    with open(outdir / "m3_weak.csv", "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(out[0]))
        w.writeheader()
        w.writerows(out)

    chips = min(n, 5000)
    print(f"\nChip level with weak defects (128 rf spares, 6 ALU spares, K=16, {chips} chips):")
    print(f"{'q':>5} {'R':>3} {'passed':>7} {'latent|passed':>14} {'failed check|passed':>20}")
    chip_rows = []
    for q in (0.1, 0.5):
        for R in (1, 4, 16, 64):
            res = run(["--mode", "yield", "--trials", str(chips), "--seed", str(SEED + 200 + R),
                       "--k", "16", "--rf", "128", "--alu", "6", "--activation", str(q),
                       "--rounds", str(R), "--verify"])[0]
            passed, lat, esc = int(res["both_ok"]), int(res["latent"]), int(res["escapes"])
            chip_rows.append(dict(activation=q, rounds=R, chips=chips, passed=passed, latent=lat, escapes=esc))
            print(f"{q:5.2f} {R:3d} {passed / chips:7.3f} {lat / max(passed, 1):14.3f} {esc / max(passed, 1):20.3f}")
    with open(outdir / "m3_weak_chips.csv", "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(chip_rows[0]))
        w.writeheader()
        w.writerows(chip_rows)
    print(f"weak-detection points outside 4 sigma: {outside}")
    return outside


def write_svg(rows, path, p8):
    """Two panels: register-file yield vs spares, and ALU yield vs spare area per K."""
    W, H, M = 760, 330, 48
    colors = {"regfile": "#1f6feb", 8: "#1f6feb", 16: "#d97706", 32: "#7c3aed"}

    def panel(x0, title, xlabel, xmax, series):
        pw, ph = 320, H - 2 * M
        g = [f'<text x="{x0}" y="22" font-size="14" font-weight="600">{title}</text>']
        for t in range(0, 11, 2):
            y = M + ph - ph * t / 10
            g.append(f'<line x1="{x0}" x2="{x0 + pw}" y1="{y}" y2="{y}" stroke="#ddd"/>'
                     f'<text x="{x0 - 6}" y="{y + 4}" font-size="10" text-anchor="end">{t / 10:.1f}</text>')
        for t in range(0, int(xmax) + 1, max(1, int(xmax) // 5)):
            x = x0 + pw * t / xmax
            g.append(f'<text x="{x}" y="{M + ph + 16}" font-size="10" text-anchor="middle">{t}</text>')
        g.append(f'<text x="{x0 + pw / 2}" y="{H - 8}" font-size="11" text-anchor="middle">{xlabel}</text>')
        for label, color, curve, pts in series:
            curve = [(x, y) for x, y in curve if x <= xmax]   # keep inside the panel
            pts = [(x, y) for x, y in pts if x <= xmax]
            d = " ".join(f"{'M' if i == 0 else 'L'}{x0 + pw * x / xmax:.1f},{M + ph - ph * y:.1f}"
                         for i, (x, y) in enumerate(curve))
            g.append(f'<path d="{d}" fill="none" stroke="{color}" stroke-width="1.5"/>')
            for x, y in pts:
                g.append(f'<circle cx="{x0 + pw * x / xmax:.1f}" cy="{M + ph - ph * y:.1f}" r="2.8" fill="{color}"/>')
        for i, (label, color, _, _) in enumerate(series):
            g.append(f'<text x="{x0 + 10}" y="{M + 14 + 14 * i}" font-size="11" fill="{color}">{label}</text>')
        return "\n".join(g)

    rf_curve = [(s, ym.file_yield_shared_pool(256, s, p8)) for s in range(0, 161, 2)]
    rf_pts = [(r["rf_spares"], r["simulated"]) for r in rows if r["kind"] == "regfile"]
    alu_series = []
    for k in (8, 16, 32):
        curve = [(100 * (ym.alu_area_ratio(k, s) - 1), ym.alu_yield(k, s, p8)) for s in range(0, 25)]
        pts = [(100 * (ym.alu_area_ratio(k, r["alu_spares"]) - 1), r["simulated"])
               for r in rows if r["kind"] == "alu" and r["k"] == k]
        alu_series.append((f"K = {k}", colors[k], curve, pts))
    svg = f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" font-family="system-ui, sans-serif">
<rect width="{W}" height="{H}" fill="white"/>
<g fill="#222">
{panel(48, "Register file: yield vs spare slices", "spare byte slices (of 256)", 160,
       [("closed form (line), simulation (dots)", colors["regfile"], rf_curve, rf_pts)])}
{panel(410, "ALU: yield vs spare area", "ALU spare area (% of the ALU)", 200, alu_series)}
</g>
<text x="{W - 6}" y="{H - 6}" font-size="9" text-anchor="end" fill="#666">simulation, tier-C p = {p8}; not a measurement</text>
</svg>'''
    path.write_text(svg)


if __name__ == "__main__":
    main()
