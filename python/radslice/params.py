"""Loader for config/params.toml that enforces the tier rules."""
from __future__ import annotations

import tomllib
from dataclasses import dataclass
from pathlib import Path

TIERS = ("A", "B", "C")
DEFAULT_PATH = Path(__file__).resolve().parents[2] / "config" / "params.toml"


@dataclass(frozen=True)
class Param:
    name: str
    value: float | int
    tier: str
    unit: str
    source: str | None
    note: str


def _walk(table: dict, prefix: str = ""):
    for key, val in table.items():
        name = f"{prefix}{key}"
        if isinstance(val, dict) and "value" in val:
            yield name, val
        elif isinstance(val, dict):
            yield from _walk(val, name + ".")


def load_params(path: Path = DEFAULT_PATH) -> dict[str, Param]:
    with open(path, "rb") as f:
        raw = tomllib.load(f)
    raw.pop("meta", None)
    out: dict[str, Param] = {}
    for name, entry in _walk(raw):
        tier = entry.get("tier")
        if tier not in TIERS:
            raise ValueError(f"{name}: tier must be one of {TIERS}, got {tier!r}")
        source = entry.get("source")
        if tier in ("A", "B") and not source:
            raise ValueError(f"{name}: tier {tier} requires a source")
        out[name] = Param(name, entry["value"], tier, entry.get("unit", ""),
                          source, entry.get("note", "").strip())
    return out
