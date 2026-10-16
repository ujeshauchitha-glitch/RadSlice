"""Transcribe the wafer map in Supplementary Fig. 7 of the Nature RISC-V MoS2 paper.

You need the paper's Supplementary Information PDF (it is not redistributed here):
Ao, Zhou et al., Nature 640, 654-661 (2025), doi:10.1038/s41586-025-08759-9, file
"41586_2025_8759_MOESM1_ESM.pdf". Requires PyMuPDF, numpy and Pillow (not needed by the
tests; the transcribed map is committed in results/si_fig7_wafer_map.txt).

    python scripts/si_fig7_extract.py path/to/41586_2025_8759_MOESM1_ESM.pdf [--write]

Method: the figure is one embedded raster. Panel (c) is a grid of 28 matrices; each is
sampled along three vertical lines through its middle and each run of green or red
pixels at least 3 px tall is one 8-bit register. Every matrix must give exactly 8
registers, or the script stops. The red count was cross-checked against a separate
connected-component count of red stripes (59 both ways).
"""
import sys
import warnings
from pathlib import Path

warnings.filterwarnings("ignore")


def main() -> None:
    import numpy as np
    import pymupdf

    pdf = sys.argv[1]
    doc = pymupdf.open(pdf)
    page = next((p for p in doc if "Supplementary Fig. 7" in p.get_text() and "Yield analysis" in p.get_text()), None)
    if page is None:
        sys.exit("could not find Supplementary Fig. 7 in this PDF")
    xref = page.get_images(full=True)[0][0]
    pix = pymupdf.Pixmap(doc, xref)
    if pix.n - pix.alpha >= 4:
        pix = pymupdf.Pixmap(pymupdf.csRGB, pix)
    a = np.frombuffer(pix.samples, dtype=np.uint8).reshape(pix.height, pix.width, pix.n)[:, :, :3].astype(int)
    nonwhite = a.min(axis=2) < 215

    def runs(mask, min_len):
        out, start = [], None
        for i, v in enumerate(list(mask) + [False]):
            if v and start is None:
                start = i
            if not v and start is not None:
                if i - start >= min_len:
                    out.append((start, i - 1))
                start = None
        return out

    y0 = 540                                   # below the legend
    bands = [(s + y0, e + y0) for s, e in runs(nonwhite[y0:, 100:800].sum(axis=1) > 0, 30)]
    boxes = [(xs, xe, ys, ye) for ys, ye in bands for xs, xe in runs(nonwhite[(ys + ye) // 2, :], 40)]
    if len(boxes) != 28:
        sys.exit(f"expected 28 matrices, found {len(boxes)}")

    def cls(px):
        r, g, b = px
        if g > r + 25 and g > b + 25:
            return "G"
        if r > g + 70 and r > b + 70:
            return "R"
        return "."

    grid = []
    for xs, xe, ys, ye in boxes:
        cx = (xs + xe) // 2
        col = []
        for y in range(ys, ye + 1):
            votes = [cls(a[y, cx + d]) for d in (-8, 0, 8)]
            col.append(max(set(votes), key=votes.count))
        regs, cur, n = [], None, 0
        for ch in col + ["."]:
            if ch == cur:
                n += 1
            else:
                if cur in ("G", "R") and n >= 3:
                    regs.append(cur)
                cur, n = ch, 1
        if len(regs) != 8:
            sys.exit(f"matrix at x={xs}, y={ys} gave {len(regs)} registers, not 8")
        grid.append("".join(regs))

    works = sum(r.count("G") for r in grid)
    print(f"28 matrices x 8 registers: {works} work, {224 - works} error, pass rate {works / 224:.4f}")
    if "--write" in sys.argv:
        out = Path(__file__).resolve().parents[1] / "results" / "si_fig7_wafer_map.txt"
        out.write_text("\n".join(grid), newline="\n")
        print("wrote", out)


if __name__ == "__main__":
    main()
