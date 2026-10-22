"""G14: the spares and their routing are made of transistors that fail too.

G10 costed the routing but treated it as fault-free. Here each spare carries its own private routing
(tag, compare, multiplexer inputs, from cost_model), which fails with the spare: a spare's yield is the
slice yield times q ** (private transistors), q being the per-transistor yield. Routing shared by all
spares (`shared_T`, a tier-C fixed block) is a single point of failure unless replicated `copies` times.

Independent defects only. Slice yield p is the register-block yield (0.71); q is a per-transistor yield:
0.99762 is implied by the register measurements (tier B), 0.9992 is the isolated-device figure (tier A).
With q = 1 this module reproduces the G10 numbers exactly (tested).
"""
from __future__ import annotations

import math
from functools import lru_cache

from . import cost_model as cost
from .cost_model import Costs

SHARED_T = cost.RF_LANES * 8 * 2 * cost.T_MUX2     # one final select per lane-bit-port: 1,152 T (tier C)


def _log_binom_pmf(n: int, k: int, p: float) -> float:
    if p <= 0.0:
        return 0.0 if k == 0 else -math.inf
    if p >= 1.0:
        return 0.0 if k == n else -math.inf
    return (math.lgamma(n + 1) - math.lgamma(k + 1) - math.lgamma(n - k + 1)
            + k * math.log(p) + (n - k) * math.log1p(-p))


def hetero_yield(primaries: int, p_prim: float, spares: int, p_spare: float) -> float:
    """P(good spares >= bad primaries), primaries good w.p. p_prim, spares good w.p. p_spare."""
    if spares < 0:
        raise ValueError("spares")
    spare_pmf = [math.exp(_log_binom_pmf(spares, g, p_spare)) for g in range(spares + 1)]
    tail = [0.0] * (spares + 2)                       # tail[j] = P(good spares >= j)
    for j in range(spares, -1, -1):
        tail[j] = tail[j + 1] + spare_pmf[j]
    total = 0.0
    for bad in range(0, min(primaries, spares) + 1):
        total += math.exp(_log_binom_pmf(primaries, bad, 1.0 - p_prim)) * tail[bad]
    return min(1.0, total)


def spare_private_T(spares: int, regions: int, c: Costs = Costs()) -> float:
    """Private routing transistors per register-file spare."""
    return cost.rf_routing_T(spares, regions, c) / spares if spares > 0 else 0.0


@lru_cache(maxsize=None)
def rf_core_yield(p: float, q: float, spares: int, regions: int,
                  topology: str = "tagged", read_ports: int = 2) -> float:
    """Slices plus the spares' private routing, without the shared block."""
    c = Costs(topology=topology, read_ports=read_ports)
    d = cost.RF_PRIMARIES // regions
    s = spares // regions
    ps = p * q ** spare_private_T(spares, regions, c)
    region_yield = hetero_yield(d, p, s, ps) if s > 0 else p ** d
    return region_yield ** regions


def shared_yield(q: float, spares: int, copies: int = 1, shared_T: float = SHARED_T) -> float:
    """At least one of `copies` copies of the shared block works (ideal selection among copies)."""
    return 1.0 - (1.0 - q ** shared_T) ** copies if spares > 0 and shared_T > 0 else 1.0


def rf_yield(p: float, q: float, spares: int, regions: int, copies: int = 1,
             topology: str = "tagged", read_ports: int = 2) -> float:
    return rf_core_yield(p, q, spares, regions, topology, read_ports) * shared_yield(q, spares, copies)


def rf_silicon_T(spares: int, regions: int, copies: int = 1, c: Costs = Costs(),
                 shared_T: float = SHARED_T) -> float:
    shared = shared_T * copies if spares > 0 else 0.0
    return cost.rf_total_T(spares, regions, c) + shared


def alu_spare_private_T(k: int, spares: int, org: str, c: Costs = Costs()) -> float:
    return cost.alu_routing_T(k, spares, org, c) / spares if spares > 0 else 0.0


@lru_cache(maxsize=None)
def alu_yield(p: float, q: float, k: int, spares: int, org: str) -> float:
    """ALU yield with spare units that carry their routing. Slice unit yield is p ** (k / 8)."""
    pu = p ** (k / 8.0)
    ps = pu * q ** alu_spare_private_T(k, spares, org)
    if org == "single":
        return 1.0 - (1.0 - pu) * (1.0 - ps) ** spares
    return hetero_yield(64 // k, pu, spares, ps)
