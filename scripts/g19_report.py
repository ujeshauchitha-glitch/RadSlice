"""G19 step 4: summary table and figure from results/g19_campaign.csv (run g19_campaign.py first)."""
import csv
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GOLD = 0.9685            # fault-free integer accuracy, results/g19_baseline.md
FLOOR = GOLD - 0.01      # the accuracy floor used here: one point below fault-free
ELEMENTS = {"none": "1x", "dwc": "2x", "tmr": "3x", "abft": "1.125x lanes + one 16-bit-weight lane",
            "abft_noreload": "1.125x lanes + one 16-bit-weight lane"}
LABEL = {"none": "no protection", "dwc": "duplicate + compare + retry", "tmr": "triple vote",
         "abft": "checksum lane + retry (clean reload)", "abft_noreload": "checksum lane + retry (no clean copy)"}
COLOR = {"none": "#888888", "dwc": "#1b9e77", "tmr": "#d95f02", "abft": "#7570b3", "abft_noreload": "#e7298a"}


def load():
    rows = []
    with open(os.path.join(ROOT, "results", "g19_campaign.csv"), newline="") as f:
        for r in csv.DictReader(f):
            for k in ("lam", "mu", "n", "accuracy", "silent_wrong", "detected_fail", "cycles"):
                r[k] = float(r[k])
            rows.append(r)
    return rows


def main():
    rows = load()
    out = ["# G19 step 4: accuracy of the protected MAC array under upsets", "",
           f"Network: 64-32-10 int8 digit classifier, fault-free accuracy {GOLD:.4f}. Each cell is {int(rows[0]['n'])} images "
           f"(the held-out set, repeated with fresh faults). Accuracy counts a detected failure as no answer. "
           f"`silent` is the fraction answered differently from the fault-free network with no warning. "
           f"Accuracy floor used here: {FLOOR:.4f} (one point below fault-free). All rates are tier C and swept.", ""]
    for exp, key, unit in (("accumulator", "lam", "accumulator upset per element per cycle"),
                           ("weight", "mu", "weight-bit upset per bit per image")):
        out += [f"## {exp} upsets ({unit})", ""]
        rates = sorted({r[key] for r in rows if r["experiment"] == exp})
        out += ["| scheme | elements | " + " | ".join(f"{x:g}" for x in rates) + " |",
                "|---|---|" + "---|" * len(rates)]
        for sch in ELEMENTS:
            cells = []
            for x in rates:
                r = next(r for r in rows if r["experiment"] == exp and r["scheme"] == sch and r[key] == x)
                cells.append(f"{r['accuracy']:.3f} / {r['silent_wrong']:.3f} / x{r['cycles']:.2f}")
            out.append(f"| {LABEL[sch]} | {ELEMENTS[sch]} | " + " | ".join(cells) + " |")
        out += ["", "Each cell: accuracy / silent wrong / cycles relative to the unprotected array.", ""]
        out += ["Highest rate at which accuracy stays at or above the floor:", ""]
        for sch in ELEMENTS:
            ok = [r[key] for r in rows if r["experiment"] == exp and r["scheme"] == sch and r["accuracy"] >= FLOOR]
            out.append(f"- {LABEL[sch]}: {max(ok):g}")
        out.append("")
    with open(os.path.join(ROOT, "results", "g19_campaign.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(out))
    print("\n".join(out))

    fig, axes = plt.subplots(2, 2, figsize=(11, 7.5), sharex="col")
    for col, (exp, key, xlabel) in enumerate((("accumulator", "lam", "accumulator upsets per element per cycle"),
                                              ("weight", "mu", "weight-bit upsets per bit per image"))):
        for sch in ELEMENTS:
            pts = sorted((r[key], r["accuracy"], r["silent_wrong"]) for r in rows
                         if r["experiment"] == exp and r["scheme"] == sch and r[key] > 0)
            axes[0][col].plot([p[0] for p in pts], [p[1] for p in pts], "o-", color=COLOR[sch], label=LABEL[sch])
            axes[1][col].plot([p[0] for p in pts], [max(p[2], 1e-4) for p in pts], "o-", color=COLOR[sch])
        axes[0][col].axhline(FLOOR, color="k", ls=":", lw=1)
        axes[0][col].set_ylabel("accuracy (detected failure = no answer)")
        axes[0][col].set_ylim(0, 1.02)
        axes[1][col].set_yscale("log")
        axes[1][col].set_ylabel("silent wrong answers (0 drawn at 1e-4)")
        axes[1][col].set_xlabel(xlabel)
        for ax in axes[:, col]:
            ax.set_xscale("log")
            ax.grid(alpha=0.3)
    axes[0][0].legend(fontsize=8, loc="lower left")
    axes[0][0].set_title("Upsets in the accumulators")
    axes[0][1].set_title("Upsets in the stored weights")
    fig.suptitle("Simulated, tier-C rates: accuracy of the 64-32-10 digit classifier on the protected MAC array "
                 "(dotted line: floor = fault-free minus 1 point)", fontsize=9)
    fig.tight_layout()
    fig.savefig(os.path.join(ROOT, "results", "g19_campaign.svg"))
    fig.savefig(os.path.join(ROOT, "results", "g19_campaign.png"), dpi=140)


if __name__ == "__main__":
    main()
