"""M6: seeds and K (G16), and the new fault classes (G15).

Simulation only; every rate is a tier-C parameter, swept, never quoted as a measurement.

Parts
  A  robustness: the M4 combined campaign repeated over 5 independent seeds and K = 8, 16, 32;
     success probability with its between-seed spread, whether the ranking of schemes is stable,
     and the fault-free cycle overhead, so correctness and cost sit on one Pareto plot
  B1 control state: PC, instruction word and CSR upsets, unprotected and with voted control state
  B2 memory: upsets in DRAM with and without SECDED, burst sizes 1, 2, 3
  B3 register-file bursts: 1, 2 and 4 adjacent bits against each register-file scheme
  B4 in-field defects: stuck-bit slices appearing after boot, with and without periodic retest
  B5 everything on, with protections added one at a time

Usage (repo root, after building radslice_inject and the workloads):
    python scripts/m6_campaign.py [--runs 400] [--jobs 12] [--quick] [--out results]
"""
import argparse
import csv
import math
import os
import statistics
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import m4_campaign as m4  # noqa: E402

ROOT = m4.ROOT
EMU = m4.EMU
WORKLOADS = m4.WORKLOADS
OUTCOMES = m4.OUTCOMES
wilson = m4.wilson

SEEDS = (11, 12, 13, 14, 15)
KS = (8, 16, 32)
A_CONFIGS = ["none", "parity+scrub", "dup+scrub", "tmr-rf+scrub", "alu-dwc", "full"]

# Rates chosen so that a run sees roughly 0.05 to 1 upset of the class (the runs are 3e4 to 1e5 instructions).
PC_RATES = [1e-8, 3e-8, 1e-7]
IR_RATES = [1e-8, 3e-8, 1e-7]
CSR_RATES = [3e-9, 1e-8, 3e-8]
MEM_RATES = [3e-11, 1e-10, 3e-10]
INFIELD_RATES = [1e-9, 3e-9, 1e-8, 1e-7, 3e-7, 1e-6]
RF_BURST_RATE = 3e-9
MEM_SCRUB = 256
RETESTS = [0, 2000, 500]


def run_cell(cell, runs, seed):
    rf, alu, scrub, _ = m4.CONFIGS[cell["config"]]
    cmd = [str(EMU), "--elf", str(ROOT / "build/workloads" / (cell["workload"] + ".elf")),
           "--rf", rf, "--alu", alu, "--scrub", str(scrub), "--k", str(cell.get("k", 16)),
           "--lambda-rf", repr(cell.get("lam_rf", 0.0)), "--lambda-alu", repr(cell.get("lam_alu", 0.0)),
           "--runs", str(runs), "--seed", str(seed), "--label", cell["part"]]
    for flag, key in (("--lambda-pc", "lam_pc"), ("--lambda-ir", "lam_ir"), ("--lambda-csr", "lam_csr"),
                      ("--lambda-mem", "lam_mem"), ("--lambda-infield", "lam_inf"), ("--burst", "burst"),
                      ("--mem-scrub", "mem_scrub"), ("--retest", "retest")):
        if cell.get(key):
            cmd += [flag, repr(cell[key])]
    for flag, key in (("--control-tmr", "control_tmr"), ("--mem-ecc", "mem_ecc")):
        if cell.get(key):
            cmd.append(flag)
    if cell.get("spares"):
        cmd += ["--spares", str(cell["spares"][0]), str(cell["spares"][1])]
    out = subprocess.run(cmd, capture_output=True, text=True)
    if out.returncode != 0:
        sys.exit(f"cell failed: {' '.join(cmd)}\n{out.stderr}")
    return next(csv.DictReader(out.stdout.splitlines()))


def build_cells():
    cells = []
    for w in WORKLOADS:
        for k in KS:
            for c in A_CONFIGS:
                for s in SEEDS:
                    cells.append(dict(part="A", workload=w, config=c, k=k, seed=s,
                                      lam_rf=m4.MID_RF, lam_alu=m4.MID_ALU))
                cells.append(dict(part="A_overhead", workload=w, config=c, k=k, seed=1, runs=1))
    for w in WORKLOADS:
        for cls, rates in (("lam_pc", PC_RATES), ("lam_ir", IR_RATES), ("lam_csr", CSR_RATES)):
            for lam in rates:
                for tmr in (False, True):
                    cells.append(dict(part="B1", workload=w, config="none", cls=cls, rate=lam,
                                      control_tmr=tmr, **{cls: lam}))
        for lam in MEM_RATES:
            for burst in (1, 2, 3):
                for ecc in (False, True):
                    cells.append(dict(part="B2", workload=w, config="none", rate=lam, burst=burst, mem_ecc=ecc,
                                      mem_scrub=MEM_SCRUB if ecc else 0, lam_mem=lam))
        for burst in (1, 2, 4):
            for c in ("none", "parity+scrub", "dup+scrub", "tmr-rf+scrub"):
                cells.append(dict(part="B3", workload=w, config=c, burst=burst, lam_rf=RF_BURST_RATE))
        for lam in INFIELD_RATES:
            for c in ("none", "tmr-rf"):
                for rt in RETESTS:
                    cells.append(dict(part="B4", workload=w, config=c, rate=lam, retest=rt, lam_inf=lam,
                                      spares=(8, 0)))
        base = dict(lam_rf=m4.MID_RF, lam_alu=m4.MID_ALU, lam_pc=3e-8, lam_ir=3e-8, lam_csr=1e-8,
                    lam_mem=1e-10, lam_inf=3e-9, spares=(8, 0))
        for name, extra in (("rf+alu protection only", {}),
                            ("+ voted control state", dict(control_tmr=True)),
                            ("+ SECDED memory", dict(control_tmr=True, mem_ecc=True, mem_scrub=MEM_SCRUB)),
                            ("+ online retest", dict(control_tmr=True, mem_ecc=True, mem_scrub=MEM_SCRUB, retest=500))):
            cells.append(dict(part="B5", workload=w, config="full", label=name, **base, **extra))
        cells.append(dict(part="B5", workload=w, config="none", label="nothing protected", **base))
    return cells


def row_for(cell, r, runs):
    n = int(r["runs"])
    counts = {o: int(r[o]) for o in OUTCOMES}
    base = float(r["baseline_cycles"])
    row = dict(part=cell["part"], workload=cell["workload"], config=cell["config"], k=cell.get("k", 16),
               seed=cell.get("seed", ""), label=cell.get("label", ""), runs=n,
               burst=cell.get("burst", 1), control_tmr=int(bool(cell.get("control_tmr"))),
               mem_ecc=int(bool(cell.get("mem_ecc"))), retest=cell.get("retest", 0),
               lam_rf=cell.get("lam_rf", 0.0), lam_alu=cell.get("lam_alu", 0.0),
               lam_pc=cell.get("lam_pc", 0.0), lam_ir=cell.get("lam_ir", 0.0), lam_csr=cell.get("lam_csr", 0.0),
               lam_mem=cell.get("lam_mem", 0.0), lam_infield=cell.get("lam_inf", 0.0),
               hang=int(r["hang"]), crash=int(r["crash"]), diverged=int(r["diverged"]),
               mean_pc=float(r["mean_pc_upsets"]), mean_ir=float(r["mean_ir_upsets"]),
               mean_csr=float(r["mean_csr_upsets"]), mean_mem=float(r["mean_mem_upsets"]),
               mean_infield=float(r["mean_infield_defects"]), mean_retests=float(r["mean_retests"]),
               mean_masked=float(r["mean_control_masked"]),
               mean_cycles_over_baseline=(float(r["mean_cycles"]) / base) if base else 0.0)
    for o in OUTCOMES:
        lo, hi = wilson(counts[o], n)
        row.update({o: counts[o], o + "_frac": counts[o] / n, o + "_lo": lo, o + "_hi": hi})
    good = counts["correct"] + counts["detected_recovered"]
    ok = n - counts["boot_failure"]
    lo, hi = wilson(good, ok) if ok else (0.0, 0.0)
    row.update(success=good / ok if ok else 0.0, success_lo=lo, success_hi=hi)
    return row


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--runs", type=int, default=400)
    ap.add_argument("--jobs", type=int, default=min(14, os.cpu_count() or 4))
    ap.add_argument("--quick", action="store_true", help="a few dozen runs, for CI")
    ap.add_argument("--out", default=str(ROOT / "results"))
    args = ap.parse_args()
    if EMU is None:
        sys.exit("build radslice_inject first (cmake --build build)")
    runs = 40 if args.quick else args.runs
    cells = build_cells()
    seeds = [cell.get("seed") or (5000 + i) for i, cell in enumerate(cells)]
    print(f"{len(cells)} cells x {runs} runs, {args.jobs} jobs", flush=True)
    with ThreadPoolExecutor(max_workers=args.jobs) as ex:
        raw = list(ex.map(lambda i: run_cell(cells[i], cells[i].get("runs", runs), seeds[i]), range(len(cells))))
    rows = [row_for(c, r, runs) for c, r in zip(cells, raw)]

    outdir = Path(args.out)
    outdir.mkdir(parents=True, exist_ok=True)
    cols = sorted({k for r in rows for k in r}, key=lambda k: list(rows[0]).index(k) if k in rows[0] else 999)
    with open(outdir / "m6_cells.csv", "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=cols)
        w.writeheader()
        w.writerows(rows)
    write_summary(rows, outdir / "m6_summary.md", runs)
    write_svg(rows, outdir / "m6_pareto.svg")
    print("written:", outdir / "m6_cells.csv", outdir / "m6_summary.md", outdir / "m6_pareto.svg")


def pct(r, o):
    return f"{100 * r[o + '_frac']:.1f}"


def mix(r):
    return (f"{pct(r, 'correct')} | {pct(r, 'detected_recovered')} | {pct(r, 'detected_unrecovered')} | "
            f"{pct(r, 'sdc')} | {pct(r, 'hang_crash')}")


MIXHEAD = "| correct % | recovered % | fail-stop % | SDC % | hang/crash % |"


def write_summary(rows, path, runs):
    L = [f"Generated by scripts/m6_campaign.py: {runs} runs per cell, fixed seeds. Simulation, tier-C rates; "
         "not a measurement.\n"]

    # ---- A: seeds and K
    L.append("## A. Seeds and K (combined register-file and ALU upsets at the M4 middle rates)\n")
    L.append("Success = correct or recovered, among runs that booted. Per scheme and K, pooled over the three "
             "workloads; `spread` is the min to max over the 5 seeds of the success probability pooled over workloads; "
             "`pooled CI` is the Wilson interval on all runs together.\n")
    L.append("| K | scheme | success % | pooled CI | spread over seeds | cycles (fault-free) | storage |")
    L.append("|---:|:--|---:|:--|:--|---:|---:|")
    overhead = {}
    for r in rows:
        if r["part"] == "A_overhead":
            overhead[(r["workload"], r["config"], r["k"])] = r["mean_cycles_over_baseline"]
    stable = []
    summary_points = []
    for k in m4_ks(rows):
        for c in A_CONFIGS:
            sel = [r for r in rows if r["part"] == "A" and r["k"] == k and r["config"] == c]
            good = sum(round(r["success"] * (r["runs"] - r["boot_failure"])) for r in sel)
            ok = sum(r["runs"] - r["boot_failure"] for r in sel)
            lo, hi = wilson(good, ok)
            per_seed = []
            for s in SEEDS:
                ss = [r for r in sel if r["seed"] == s]
                g = sum(round(r["success"] * (r["runs"] - r["boot_failure"])) for r in ss)
                o = sum(r["runs"] - r["boot_failure"] for r in ss)
                per_seed.append(g / o)
            ovh = statistics.mean(overhead[(w, c, k)] for w in WORKLOADS)
            stor = m4.CONFIGS[c][3]
            L.append(f"| {k} | {c} | {100 * good / ok:.1f} | {100 * lo:.1f}-{100 * hi:.1f} | "
                     f"{100 * min(per_seed):.1f}-{100 * max(per_seed):.1f} | {ovh:.2f}x | {stor:.2f}x |")
            summary_points.append((k, c, good / ok, ovh, stor, per_seed))
    L.append("")
    L.append("Is the ranking of schemes the same in every seed? For each K and seed, schemes are ordered by success "
             "probability (pooled over workloads); a pair counts as stable if one scheme is ahead in all 5 seeds.\n")
    L.append("| K | pairs with a consistent order in all seeds | pairs whose order flips between seeds |")
    L.append("|---:|---:|:--|")
    for k in m4_ks(rows):
        pts = {c: ps for (kk, c, _, _, _, ps) in summary_points if kk == k}
        consistent, flips = 0, []
        names = list(pts)
        for i in range(len(names)):
            for j in range(i + 1, len(names)):
                a, b = pts[names[i]], pts[names[j]]
                if all(x > y for x, y in zip(a, b)) or all(x < y for x, y in zip(a, b)):
                    consistent += 1
                else:
                    flips.append(f"{names[i]} vs {names[j]}")
        L.append(f"| {k} | {consistent} of {consistent + len(flips)} | {'; '.join(flips) or 'none'} |")
    L.append("")

    # ---- B1
    L.append("## B1. Control-state upsets (PC: 64 bits, instruction word: 32 bits, CSRs: 5 x 64 bits)\n")
    L.append("Unprotected register file and ALU; the only change is whether control state is voted (every upset masked).\n")
    L.append("| workload | class | rate | voted | mean upsets/run |" + MIXHEAD[1:])
    L.append("|:--|:--|---:|:-:|---:|---:|---:|---:|---:|---:|")
    for r in rows:
        if r["part"] == "B1":
            cls = "pc" if r["lam_pc"] else "ir" if r["lam_ir"] else "csr"
            rate = r["lam_pc"] or r["lam_ir"] or r["lam_csr"]
            up = r["mean_" + cls]
            L.append(f"| {r['workload']} | {cls} | {rate:g} | {'yes' if r['control_tmr'] else 'no'} | {up:.3f} | {mix(r)} |")
    L.append("")

    # ---- B2
    L.append("## B2. Memory upsets and SECDED\n")
    L.append("Upsets land on the memory the program actually touches (its footprint: 349, 253 and 123 words of 8 bytes "
             "for coremark_like, chacha20 and irq_loop), at the stated rate per bit of that footprint; upsets in "
             "memory a program never uses cannot matter and are not simulated. SECDED: one flip in a 64-bit word is "
             "corrected when the word is read, written or scrubbed; two are detected and stop the machine only when the "
             "word is read; three or more escape (a worst case). The scrub (every 256 instructions) corrects pending "
             "single flips so they do not pair up. A protection that fires counts as recovered.\n")
    L.append("| workload | rate | burst | SECDED | mean upsets/run |" + MIXHEAD[1:])
    L.append("|:--|---:|---:|:-:|---:|---:|---:|---:|---:|---:|")
    for r in rows:
        if r["part"] == "B2":
            L.append(f"| {r['workload']} | {r['lam_mem']:g} | {r['burst']} | {'yes' if r['mem_ecc'] else 'no'} | "
                     f"{r['mean_mem']:.2f} | {mix(r)} |")
    L.append("")

    # ---- B3
    L.append(f"## B3. Register-file bursts ({RF_BURST_RATE:g} events per bit per cycle, adjacent bits in one word)\n")
    L.append("| workload | scheme | burst | mean events/run |" + MIXHEAD[1:])
    L.append("|:--|:--|---:|---:|---:|---:|---:|---:|---:|")
    for r in rows:
        if r["part"] == "B3":
            L.append(f"| {r['workload']} | {r['config']} | {r['burst']} | n/a | {mix(r)} |")
    L.append("")

    # ---- B4
    L.append("## B4. Defects appearing after boot (stuck bit in a register-file slice, 8 spares per copy)\n")
    L.append("`retest` is the period, in instructions, of an online self-test and remap (0 = never); each costs "
             f"{m4_retest_cycles()} model cycles per physical slice per copy.\n")
    L.append("| workload | scheme | rate per slice-cycle | retest | mean defects/run | mean retests/run | cycles |" + MIXHEAD[1:])
    L.append("|:--|:--|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
    for r in rows:
        if r["part"] == "B4":
            L.append(f"| {r['workload']} | {r['config']} | {r['lam_infield']:g} | {r['retest']} | "
                     f"{r['mean_infield']:.3f} | {r['mean_retests']:.1f} | {r['mean_cycles_over_baseline']:.2f}x | {mix(r)} |")
    L.append("")

    # ---- B5
    L.append("## B5. All fault classes at once, protections added in turn\n")
    L.append("Rates: register file 3e-9, ALU 2e-6, PC 3e-8, instruction 3e-8, CSR 1e-8, memory 1e-10, in-field "
             "defects 3e-9. Register file TMR with scrub and ALU DWC throughout except the first row.\n")
    L.append("| workload | protections | success % (95% CI) |" + MIXHEAD[1:])
    L.append("|:--|:--|:--|---:|---:|---:|---:|---:|")
    for r in rows:
        if r["part"] == "B5":
            L.append(f"| {r['workload']} | {r['label']} | {100 * r['success']:.1f} "
                     f"({100 * r['success_lo']:.1f}-{100 * r['success_hi']:.1f}) | {mix(r)} |")
    L.append("")
    path.write_text("\n".join(L), encoding="utf-8")


def m4_ks(rows):
    return sorted({r["k"] for r in rows if r["part"] == "A"})


def m4_retest_cycles():
    return 8


def write_svg(rows, path):
    W, H, M = 640, 360, 54
    pw, ph = W - 2 * M, H - 2 * M
    pts = {}
    ov = {}
    for r in rows:
        if r["part"] == "A_overhead":
            ov.setdefault((r["config"], r["k"]), []).append(r["mean_cycles_over_baseline"])
    for r in rows:
        if r["part"] == "A":
            pts.setdefault((r["config"], r["k"]), []).append((r["success"], r["runs"] - r["boot_failure"]))
    colors = {"none": "#757575", "parity+scrub": "#f9a825", "dup+scrub": "#1f6feb", "tmr-rf+scrub": "#0f9d58",
              "alu-dwc": "#7c3aed", "full": "#c62828"}
    shapes = {8: "circle", 16: "square", 32: "triangle"}
    g = ['<text x="10" y="20" font-size="14" font-weight="600">Success against cycle overhead, by scheme and K</text>']
    xs = [statistics.mean(v) for v in ov.values()]
    xmax = max(xs) * 1.05
    g.append(f'<line x1="{M}" y1="{M + ph}" x2="{M + pw}" y2="{M + ph}" stroke="#888"/>'
             f'<line x1="{M}" y1="{M}" x2="{M}" y2="{M + ph}" stroke="#888"/>')
    for t in range(0, 11, 2):
        y = M + ph - ph * t / 10
        g.append(f'<line x1="{M}" x2="{M + pw}" y1="{y}" y2="{y}" stroke="#eee"/>'
                 f'<text x="{M - 6}" y="{y + 4}" font-size="10" text-anchor="end">{t / 10:.1f}</text>')
    g.append(f'<text x="{M + pw / 2}" y="{H - 8}" font-size="11" text-anchor="middle">fault-free cycles over the unsliced baseline</text>')
    for (c, k), v in pts.items():
        good = sum(s * n for s, n in v) / sum(n for _, n in v)
        x = M + pw * statistics.mean(ov[(c, k)]) / xmax
        y = M + ph - ph * good
        col = colors[c]
        if shapes[k] == "circle":
            g.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="5" fill="{col}"/>')
        elif shapes[k] == "square":
            g.append(f'<rect x="{x - 5:.1f}" y="{y - 5:.1f}" width="10" height="10" fill="{col}"/>')
        else:
            g.append(f'<path d="M{x:.1f},{y - 6:.1f} L{x + 6:.1f},{y + 5:.1f} L{x - 6:.1f},{y + 5:.1f} Z" fill="{col}"/>')
    for i, (c, col) in enumerate(colors.items()):
        g.append(f'<text x="{W - 130}" y="{M + 12 + 13 * i}" font-size="10" fill="{col}">{c}</text>')
    g.append(f'<text x="{W - 130}" y="{M + 12 + 13 * 6 + 4}" font-size="10" fill="#444">circle K=8, square 16, triangle 32</text>')
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" font-family="system-ui, sans-serif">'
           f'<rect width="{W}" height="{H}" fill="white"/><g fill="#222">' + "\n".join(g) +
           '</g><text x="634" y="354" font-size="9" text-anchor="end" fill="#666">simulation, tier-C rates, not a measurement</text></svg>')
    path.write_text(svg, encoding="utf-8")


if __name__ == "__main__":
    main()
