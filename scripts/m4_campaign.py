"""M4: fault-injection campaigns.

Simulation only. Upset rates are tier-C parameters (config/params.toml), swept, never
quoted as measurements. Every cell is `--runs` independent runs of one workload on one
protection scheme at one upset rate; each run ends in exactly one outcome class and
the outcome proportions get Wilson 95% intervals. Seeds are fixed, so a re-run gives
identical numbers.

Campaigns
  rf        register-file upsets only, rate swept, every register-file scheme
  alu       ALU upsets only, rate swept, every ALU scheme
  combined  both, at the middle rates, every scheme
  defects   permanent defects (M3 self-test and remap) plus upsets: boot failure vs
            spares, per register-file scheme

Usage (repo root, after building radslice_inject and the workloads):
    python scripts/m4_campaign.py [--runs 2000] [--jobs 12] [--quick] [--out results]
"""
import argparse
import csv
import math
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EMU = next((p for p in (ROOT / "build/radslice_inject", ROOT / "build/radslice_inject.exe") if p.exists()), None)
WORKLOADS = ["coremark_like", "chacha20", "irq_loop"]

OUTCOMES = ["correct", "detected_recovered", "detected_unrecovered", "sdc", "hang_crash", "boot_failure"]

# name -> (rf scheme, alu scheme, scrub period, register-file storage factor)
CONFIGS = {
    "none":          ("none",       "none", 0,  1.0),
    "parity":        ("parity",     "none", 0,  65 / 64),
    "parity+scrub":  ("parity",     "none", 32, 65 / 64),
    "dup":           ("parity-dup", "none", 0,  2 * 65 / 64),
    "dup+scrub":     ("parity-dup", "none", 32, 2 * 65 / 64),
    "tmr-rf":        ("tmr",        "none", 0,  3.0),
    "tmr-rf+scrub":  ("tmr",        "none", 32, 3.0),
    "alu-dwc":       ("none",       "dwc",  0,  1.0),
    "alu-tmr":       ("none",       "tmr",  0,  1.0),
    "full":          ("tmr",        "dwc",  32, 3.0),
}
RF_CONFIGS = ["none", "parity", "parity+scrub", "dup", "dup+scrub", "tmr-rf", "tmr-rf+scrub", "full"]
ALU_CONFIGS = ["none", "alu-dwc", "alu-tmr", "full"]
EXPOSED = {"none": 31 * 64, "parity": 31 * 65, "parity-dup": 2 * 31 * 65, "tmr": 3 * 31 * 64}

RF_RATES = [1e-9, 3e-9, 1e-8]       # per bit per model cycle
ALU_RATES = [5e-7, 2e-6, 8e-6]      # per output bit per beat
MID_RF, MID_ALU = 3e-9, 2e-6


def wilson(k, n, z=1.96):
    if n == 0:
        return 0.0, 0.0
    p = k / n
    d = 1 + z * z / n
    c = (p + z * z / (2 * n)) / d
    h = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / d
    return max(0.0, c - h), min(1.0, c + h)


def run_cell(cell, runs, seed):
    rf, alu, scrub, _ = CONFIGS[cell["config"]]
    cmd = [str(EMU), "--elf", str(ROOT / "build/workloads" / (cell["workload"] + ".elf")),
           "--rf", rf, "--alu", alu, "--scrub", str(scrub),
           "--lambda-rf", repr(cell["lam_rf"]), "--lambda-alu", repr(cell["lam_alu"]),
           "--runs", str(runs), "--seed", str(seed), "--label", cell["campaign"]]
    if cell.get("defects"):
        p, rs, as_ = cell["defects"]
        cmd += ["--defects", str(p), str(rs), str(as_)]
    out = subprocess.run(cmd, capture_output=True, text=True)
    if out.returncode != 0:
        sys.exit(f"cell failed: {' '.join(cmd)}\n{out.stderr}")
    return next(csv.DictReader(out.stdout.splitlines()))


def build_cells():
    cells = []
    for w in WORKLOADS:
        for lam in RF_RATES:
            for c in RF_CONFIGS:
                cells.append(dict(campaign="rf", workload=w, config=c, lam_rf=lam, lam_alu=0.0))
        for lam in ALU_RATES:
            for c in ALU_CONFIGS:
                cells.append(dict(campaign="alu", workload=w, config=c, lam_rf=0.0, lam_alu=lam))
        for c in CONFIGS:
            cells.append(dict(campaign="combined", workload=w, config=c, lam_rf=MID_RF, lam_alu=MID_ALU))
    for spares in (96, 112, 128, 160):
        for c in ("none", "dup", "tmr-rf"):
            cells.append(dict(campaign="defects", workload="coremark_like", config=c,
                              lam_rf=MID_RF, lam_alu=MID_ALU, defects=(0.71, spares, 12)))
    # Fault-free runs, one per scheme: the model-cycle overhead of the scheme itself.
    # (The mean cycles of a faulty cell mixes runs that stopped early with runs that
    # hung, so it is not an overhead and is not reported as one.)
    for w in WORKLOADS:
        for c in CONFIGS:
            cells.append(dict(campaign="overhead", workload=w, config=c, lam_rf=0.0, lam_alu=0.0))
    return cells


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--runs", type=int, default=2000)
    ap.add_argument("--jobs", type=int, default=min(12, os.cpu_count() or 4))
    ap.add_argument("--quick", action="store_true", help="a few hundred runs, for CI")
    ap.add_argument("--out", default=str(ROOT / "results"))
    args = ap.parse_args()
    if EMU is None:
        sys.exit("build radslice_inject first (cmake --build build)")
    if not (ROOT / "build/workloads/chacha20.elf").exists():
        sys.exit("build the workloads first: bash scripts/build-workloads.sh")
    runs = 300 if args.quick else args.runs

    cells = build_cells()
    seeds = {i: 1000 + i for i in range(len(cells))}
    print(f"{len(cells)} cells x {runs} runs, {args.jobs} jobs", flush=True)
    with ThreadPoolExecutor(max_workers=args.jobs) as ex:
        rows = list(ex.map(lambda i: run_cell(cells[i], 1 if cells[i]["campaign"] == "overhead" else runs,
                                               seeds[i]), range(len(cells))))

    overhead = {}
    for cell, r in zip(cells, rows):
        if cell["campaign"] == "overhead":
            overhead[(cell["workload"], cell["config"])] = float(r["mean_cycles"]) / float(r["baseline_cycles"])

    out = []
    warn = 0
    for cell, r in zip(cells, rows):
        n = int(r["runs"])
        counts = {o: int(r[o]) for o in OUTCOMES}
        rf_mode, alu_mode, scrub, storage = CONFIGS[cell["config"]]
        base = float(r["baseline_cycles"])
        row = dict(campaign=cell["campaign"], workload=cell["workload"], config=cell["config"],
                   rf=rf_mode, alu=alu_mode, scrub=scrub, lambda_rf=cell["lam_rf"], lambda_alu=cell["lam_alu"],
                   defects=("p=%s,rf_spares=%d,alu_spares=%d" % cell["defects"]) if cell.get("defects") else "",
                   runs=n, hang=int(r["hang"]), crash=int(r["crash"]), diverged=int(r["diverged"]),
                   mean_rf_upsets=float(r["mean_rf_upsets"]), mean_alu_upsets=float(r["mean_alu_upsets"]),
                   cycle_overhead_fault_free=overhead[(cell["workload"], cell["config"])],
                   mean_cycles_all_runs_over_baseline=(float(r["mean_cycles"]) / base) if base else 0.0,
                   rf_storage_factor=storage)
        for o in OUTCOMES:
            lo, hi = wilson(counts[o], n)
            row.update({o: counts[o], o + "_frac": counts[o] / n, o + "_lo": lo, o + "_hi": hi})
        ok_runs = n - counts["boot_failure"]
        good = counts["correct"] + counts["detected_recovered"]
        lo, hi = wilson(good, ok_runs) if ok_runs else (0.0, 0.0)
        row.update(visible_failure_frac=(1 - good / ok_runs) if ok_runs else 0.0,
                   success_lo=lo, success_hi=hi)
        # Sanity: injected RF upsets per run should average rate * exposed bits * cycles
        # actually run (runs that failed to boot inject nothing). Poisson z-test.
        # Runs stop early (a crash, a fail-stop), so compare total events with rate x total
        # exposure, not per-run means: events over the cycles actually run, with Poisson error.
        events = row["mean_rf_upsets"] * n
        exposure = cell["lam_rf"] * EXPOSED[rf_mode] * float(r["mean_cycles"]) * ok_runs
        if exposure > 5:
            z = (events - exposure) / math.sqrt(max(events, 1.0))
            row["rf_upset_z"] = z
            if abs(z) > 4:
                warn += 1
                print(f"  note: injected RF upsets off by z={z:.1f}: {cell['workload']} {cell['config']} "
                      f"lambda_rf={cell['lam_rf']} observed {events:.0f} expected {exposure:.0f}")
        else:
            row["rf_upset_z"] = ""
        out.append(row)

    outdir = Path(args.out)
    outdir.mkdir(parents=True, exist_ok=True)
    with open(outdir / "m4_cells.csv", "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(out[0]))
        w.writeheader()
        w.writerows(out)
    write_summary(out, outdir / "m4_summary.md", runs)
    write_svg(out, outdir / "m4_combined.svg")
    print(f"written: {outdir / 'm4_cells.csv'}, m4_summary.md, m4_combined.svg  ({warn} rate notes)")


def pct(r, o):
    return f"{100 * r[o + '_frac']:.1f}"


def write_summary(rows, path, runs):
    lines = [f"Generated by scripts/m4_campaign.py: {runs} runs per cell, fixed seeds. "
             "Simulation, tier-C rates; not a measurement.\n"]

    def table(title, sel):
        lines.append(f"### {title}\n")
        lines.append("| workload | scheme | correct | recovered | fail-stop | **SDC** (95% CI) | hang/crash | boot fail | cycles (fault-free) | RF storage |")
        lines.append("|---|---|---:|---:|---:|---|---:|---:|---:|---:|")
        for r in sel:
            scheme = r["config"] + (f" [{r['defects']}]" if r["defects"] else "")
            lines.append(f"| {r['workload']} | {scheme} | {pct(r, 'correct')} | {pct(r, 'detected_recovered')} | "
                         f"{pct(r, 'detected_unrecovered')} | **{pct(r, 'sdc')}** ({100*r['sdc_lo']:.1f}-{100*r['sdc_hi']:.1f}) | "
                         f"{pct(r, 'hang_crash')} | {pct(r, 'boot_failure')} | {r['cycle_overhead_fault_free']:.2f}x | {r['rf_storage_factor']:.2f}x |")
        lines.append("")

    table(f"Combined upsets (register file {MID_RF:g} per bit-cycle, ALU {MID_ALU:g} per bit-beat)",
          [r for r in rows if r["campaign"] == "combined"])
    for lam in RF_RATES:
        table(f"Register-file upsets only, {lam:g} per bit per cycle",
              [r for r in rows if r["campaign"] == "rf" and r["lambda_rf"] == lam])
    for lam in ALU_RATES:
        table(f"ALU upsets only, {lam:g} per output bit per beat",
              [r for r in rows if r["campaign"] == "alu" and r["lambda_alu"] == lam])
    table("Permanent defects (p = 0.71, 12 ALU spares) plus upsets: boot failure versus register-file spares",
          [r for r in rows if r["campaign"] == "defects"])
    path.write_text("\n".join(lines))


def write_svg(rows, path):
    colors = {"correct": "#2e7d32", "detected_recovered": "#7cb342", "detected_unrecovered": "#f9a825",
              "sdc": "#c62828", "hang_crash": "#6a1b9a", "boot_failure": "#757575"}
    sel = [r for r in rows if r["campaign"] == "combined"]
    W, rowh, left, pw = 820, 20, 150, 430
    panels = WORKLOADS
    H = 40 + len(panels) * (len(CONFIGS) * rowh + 46) + 40
    g = [f'<rect width="{W}" height="{H}" fill="white"/>',
         '<text x="10" y="20" font-size="14" font-weight="600" fill="#222">Outcome mix, combined register-file and ALU upsets (simulation, tier-C rates)</text>']
    y = 44
    for w in panels:
        g.append(f'<text x="10" y="{y}" font-size="12" font-weight="600" fill="#222">{w}</text>')
        y += 8
        for cname in CONFIGS:
            r = next(x for x in sel if x["workload"] == w and x["config"] == cname)
            g.append(f'<text x="{left - 6}" y="{y + 14}" font-size="11" text-anchor="end" fill="#222">{cname}</text>')
            x = left
            for o in OUTCOMES:
                wpx = pw * r[o + "_frac"]
                if wpx > 0:
                    g.append(f'<rect x="{x:.1f}" y="{y + 2}" width="{wpx:.1f}" height="{rowh - 4}" fill="{colors[o]}"/>')
                x += wpx
            g.append(f'<text x="{left + pw + 8}" y="{y + 14}" font-size="10" fill="#555">SDC {100*r["sdc_frac"]:.1f}%  fail {100*r["visible_failure_frac"]:.0f}%  {r["cycle_overhead_fault_free"]:.2f}x cyc</text>')
            y += rowh
        y += 30
    lx = left
    for o in OUTCOMES:
        g.append(f'<rect x="{lx}" y="{y}" width="12" height="12" fill="{colors[o]}"/>'
                 f'<text x="{lx + 16}" y="{y + 11}" font-size="10" fill="#222">{o}</text>')
        lx += 125
    path.write_text(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" font-family="system-ui, sans-serif">\n'
                    + "\n".join(g) + "\n</svg>")


if __name__ == "__main__":
    main()
