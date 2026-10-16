"""Render docs/assumptions.md from config/params.toml.

The register of assumptions is generated, not hand-written, so it cannot drift from the
parameters the code actually reads. A test fails if the committed document differs from
what this renders.

    python -m radslice.assumptions            # print
    python -m radslice.assumptions --write    # update docs/assumptions.md
"""
from __future__ import annotations

import sys
from pathlib import Path

from .params import DEFAULT_PATH, load_params

DOC_PATH = Path(__file__).resolve().parents[2] / "docs" / "assumptions.md"

HEADER = """# Assumptions register

Generated from `config/params.toml` by `python -m radslice.assumptions --write`; do not edit by
hand (a test fails if this file drifts from the parameters).

Tiers: **A** published measurement (source given), **B** calibrated model (source given),
**C** hypothetical. A simulation that uses any tier-C value is a what-if, not a measurement.
"""


def _first_sentences(note: str, limit: int = 260) -> str:
    text = " ".join(note.split())
    if len(text) <= limit:
        return text
    cut = text.rfind(". ", 0, limit)
    return text[: cut + 1] if cut > 60 else text[:limit].rstrip() + "..."


def render(path: Path = DEFAULT_PATH) -> str:
    params = load_params(path)
    counts = {"A": 0, "B": 0, "C": 0}
    for p in params.values():
        counts[p.tier] += 1
    lines = [HEADER, f"**{len(params)} parameters: {counts['A']} tier A, {counts['B']} tier B, "
                     f"{counts['C']} tier C.**", ""]
    sourced = [n for n in sorted(params) if params[n].tier in ("A", "B")]
    if not sourced:
        lines += ["No parameter has been checked against a primary source. Every result in "
                  "`docs/results.md` is conditional on tier-C values.", ""]
    else:
        lines += [f"{len(sourced)} parameters have a source: " + ", ".join(f"`{n}`" for n in sourced) +
                  ". All come from one paper's Supplementary Information (Supplementary Fig. 7); its "
                  "main text was not read. Every other parameter is tier C, and every result in "
                  "`docs/results.md` is still conditional on those.", ""]
        lines += ["| sourced parameter | source |", "|---|---|"]
        for n in sourced:
            lines.append(f"| `{n}` | {params[n].source.replace('|', '\\|')} |")
        lines.append("")
    lines += ["| parameter | value | unit | tier | note |", "|---|---|---|---|---|"]
    for name in sorted(params):
        p = params[name]
        note = _first_sentences(p.note).replace("|", "\\|")
        value = str(p.value).replace("|", "\\|")
        lines.append(f"| `{name}` | {value} | {p.unit} | {p.tier} | {note} |")
    return "\n".join(lines) + "\n"


if __name__ == "__main__":
    text = render()
    if "--write" in sys.argv:
        DOC_PATH.write_text(text, encoding="utf-8", newline="\n")
        print(f"wrote {DOC_PATH}")
    else:
        print(text)
