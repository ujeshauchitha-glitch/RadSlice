"""Scalar model of rtl/mac_array.v with the same semantics (stuck bits on the stored accumulator, one flip hook).

Used to produce the expected values for the RTL testbenches that run with faults, so the Verilog and this model are
two separately written descriptions of one behaviour. The campaigns in g19_campaign.py use a batched version of
the same rules; scripts/run-mac-tests.sh checks the Verilog against this model on defective chips.
"""
import numpy as np

MASK32 = 0xFFFFFFFF


def run_tile(xs, wm, lane_map, smask, sval, flip=None):
    """xs: (n,) ints; wm: (n, P) lane weights; lane_map: (P,) physical element per lane; smask/sval: (PH,) uint32.
    flip: None or (step_index, element, bit). Returns the P lane results as signed int32."""
    n, p = wm.shape
    ph = len(smask)
    acc = [(0 & ~int(smask[e]) | int(sval[e]) & int(smask[e])) & MASK32 for e in range(ph)]  # after the clear cycle
    elem_lane = {int(lane_map[l]): l for l in range(p)}
    for k in range(n):
        for e in range(ph):
            w = int(wm[k, elem_lane[e]]) if e in elem_lane else 0
            nxt = (acc[e] + int(xs[k]) * w) & MASK32
            if flip is not None and flip[0] == k and flip[1] == e:
                nxt ^= 1 << flip[2]
            acc[e] = (nxt & ~int(smask[e]) | int(sval[e]) & int(smask[e])) & MASK32
    out = []
    for l in range(p):
        v = acc[int(lane_map[l])]
        out.append(v - (1 << 32) if v >= 1 << 31 else v)
    return out
