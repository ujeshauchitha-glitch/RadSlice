"""G10: silicon cost of the spare-routing hardware, in transistors.

Transistor count stands in for area. Wiring, vias and layout are NOT counted; a crossbar is
wire-dominated, so every cost here is more likely too low than too high.

Published anchors (Ao, Zhou et al., Nature 640, 654-661, 2025):
  * 144 transistors per 8-bit register block            (main text p. 659)
  * 27 transistors for a 1-bit adder with control logic  (Extended Data Fig. 5)
  * 27 transistors for a four-input selection module     (Extended Data Fig. 5)
Derived (tier B): a 2-input multiplexer is a third of the 4-input module, 9 transistors, because
a four-input selection is three 2-input selections. Everything else is a tier-C knob in `Costs`.

Two register-file steering topologies:
  tagged        spare slices live in a small side array. Each spare holds a tag naming the slice it
                replaces, every access compares its address against all tags, and the output lane
                multiplexers gain one input per spare that can reach the lane. Cost grows with the
                number of spares, not with the size of the register file.
  per_position  every slice position has its own multiplexer choosing between itself and the
                spares in reach. The naive crossbar; an upper bound on routing cost.
Spare reach: with `regions` regions, a spare serves only its own region. Regions are cut by byte
lane first (up to 8), then by groups of registers within a lane.
"""
from __future__ import annotations

import math
from dataclasses import dataclass

T_RF_BLOCK = 144          # transistors per 8-bit register block (published)
T_ADD_BIT = 27            # 1-bit adder with control (published)
T_SEL4 = 27               # 4-input selection module (published)
T_MUX2 = T_SEL4 / 3.0     # derived: 9 transistors per 2-input multiplexer
T_STORE_BIT = T_RF_BLOCK / 8.0   # 18 transistors per stored bit (published block / 8)

RF_PRIMARIES = 256        # 32 registers x 8 byte slices
RF_LANES = 8


@dataclass(frozen=True)
class Costs:
    topology: str = "tagged"          # "tagged" or "per_position"
    read_ports: int = 2               # register-file read ports that need steering
    write_ports: int = 1
    tag_compare_T: float = 9.0        # per tag bit per port, tier C (one XNOR-class gate)
    alu_factor: float = 1.0           # a real ALU is bigger than an adder; tier C
    other_T: float = 0.0              # silicon outside the register file and ALU; tier C
    t_mux2: float = T_MUX2
    routing_scale: float = 1.0        # 0 = ideal free crossbar (the assumption of G1-G9)


def mux_T(inputs: int, c: Costs) -> float:
    """Transistors for an `inputs`-way multiplexer built from 2-input ones."""
    return 0.0 if inputs <= 1 else c.t_mux2 * (inputs - 1)


# ---- register file -------------------------------------------------------------------------

def rf_storage_T(spares: int) -> float:
    return (RF_PRIMARIES + spares) * T_RF_BLOCK


def rf_routing_T(spares: int, regions: int, c: Costs = Costs()) -> float:
    """Steering plus tag/remap table for `spares` spares split over `regions` equal regions."""
    if spares <= 0 or c.routing_scale == 0:
        return 0.0
    return c.routing_scale * _rf_routing(spares, regions, c)


def _rf_routing(spares: int, regions: int, c: Costs) -> float:
    if c.topology == "per_position":
        reach = spares // regions
        return RF_PRIMARIES * 8 * c.read_ports * mux_T(reach + 1, c)
    if c.topology != "tagged":
        raise ValueError(c.topology)
    lanes = max(1, RF_LANES // regions)                       # lanes one spare can reach
    steering = c.read_ports * spares * lanes * 8 * c.t_mux2   # one extra mux input per bit per lane
    tag_bits = math.ceil(math.log2(max(2, RF_PRIMARIES // regions))) + 1   # +1 valid bit
    table = spares * tag_bits * (T_STORE_BIT + c.tag_compare_T * (c.read_ports + c.write_ports))
    return steering + table


def rf_total_T(spares: int, regions: int, c: Costs = Costs()) -> float:
    return rf_storage_T(spares) + rf_routing_T(spares, regions, c)


# ---- ALU -----------------------------------------------------------------------------------

def alu_units(k: int, spares: int, organisation: str) -> int:
    if organisation == "replicated":
        return 64 // k + spares
    if organisation == "single":
        return 1 + spares
    raise ValueError(organisation)


def alu_unit_T(k: int, c: Costs = Costs()) -> float:
    return c.alu_factor * T_ADD_BIT * k


def alu_routing_T(k: int, spares: int, organisation: str, c: Costs = Costs()) -> float:
    """Routing that lets a spare unit stand in for a bad one.

    replicated: a spare must take any position's operands (2K data + carry + 4 control lines in)
                and every position's K+1 result lines choose between own unit and each spare.
    single:     all units see the same operand bus, so only the K+1 result lines choose."""
    if spares <= 0 or c.routing_scale == 0:
        return 0.0
    return c.routing_scale * _alu_routing(k, spares, organisation, c)


def _alu_routing(k: int, spares: int, organisation: str, c: Costs) -> float:
    out = (k + 1) * mux_T(spares + 1, c)
    if organisation == "single":
        return out
    positions = 64 // k
    return spares * (2 * k + 5) * mux_T(positions, c) + positions * out


def alu_total_T(k: int, spares: int, organisation: str, c: Costs = Costs()) -> float:
    return alu_units(k, spares, organisation) * alu_unit_T(k, c) + alu_routing_T(k, spares, organisation, c)


def cost_per_good(silicon_T: float, yield_: float) -> float:
    """Silicon spent per working chip: every die is paid for, only yield_ of them work."""
    return float("inf") if yield_ <= 0 else silicon_T / yield_
