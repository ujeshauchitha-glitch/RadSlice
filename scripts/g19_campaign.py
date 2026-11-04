"""G19 step 4: upsets and protection on the MAC array, measured as classification accuracy.

The network is the 64-32-10 int8 digit classifier of g19_reference.py, run on a P = 8 lane array (layer 1 in four
passes of 64 steps, layer 2 in two passes of 32). Requantization between layers and the argmax are done by the
controller and are assumed fault-free. A batched model of the array does the accumulation; before any number is
reported, (a) with no faults every scheme must reproduce the integer reference exactly, and (b) a forced single
flip must give the same lane results as the scalar model (g19_array_model.py), which the Verilog matches
(scripts/run-mac-selftest.sh).

Fault classes (both tier C, swept, never claimed as measured):
  accumulator upsets: per element per cycle, probability lam, flipping one random bit of the 32-bit accumulator;
  weight upsets:      per stored weight bit per image, probability mu, in the weight memory used for one image.

Schemes:
  none           P lanes.
  dwc            two copies, compare after each pass, re-execute on mismatch (reloading weights from a clean master).
  tmr            three copies, bitwise majority vote of each lane's final accumulator; no retry.
  abft           P lanes plus a checksum lane whose 16-bit weights are the column sums of the lane weights; after a pass
                 sum(lane accumulators) must equal the checksum accumulator (mod 2^32); on mismatch, re-execute with a
                 reload of the weights from a clean master.
  abft_noreload  as abft, but a retry reuses the corrupted weights (no clean master).
A pass is retried at most R times; a pass still failing after R retries marks the image as a detected failure.
The voters, comparators, checksum adders and reload path are assumed fault-free.

usage: python scripts/g19_campaign.py [--trials T] [--quick]
"""
import argparse
import os
import zlib
import numpy as np

import g19_array_model as am

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
P = 8
R = 2
MASK = 0xFFFFFFFF


def to_signed32(a):
    a = a & MASK
    return np.where(a >= (1 << 31), a - (1 << 32), a)


def flip_ints(vals, nbits, mu, rng):
    """XOR each element of vals (two's complement, nbits wide) with independent bit flips of probability mu."""
    if mu <= 0:
        return vals
    mask = np.zeros(vals.shape, dtype=np.int64)
    for b in range(nbits):
        mask |= (rng.random(vals.shape) < mu).astype(np.int64) << b
    u = (vals.astype(np.int64) & ((1 << nbits) - 1)) ^ mask
    return np.where(u >= (1 << (nbits - 1)), u - (1 << nbits), u)


def run_pass(xb, W, scheme, lam, mu, rng, force=None):
    """One pass of the array: lanes l = 0..P-1 compute sum_k xb[:, k] * W[l, k]. xb: (B, n); W: (P, n) clean weights.
    Returns (acc (B, P) int64 signed, failed (B,) bool, steps (B,) int)."""
    B, n = xb.shape
    copies = {"none": 1, "dwc": 2, "tmr": 3, "abft": 1, "abft_noreload": 1}[scheme]
    lanes = P + (1 if scheme.startswith("abft") else 0)
    Wclean = W.astype(np.int64)
    chk_clean = Wclean.sum(0)                                   # (n,) column sums, up to 16 bits signed
    # weights as stored for this image (shared by every copy): one draw per image
    Wf = np.broadcast_to(Wclean, (B, P, n)).copy()
    Cf = np.broadcast_to(chk_clean, (B, n)).copy()
    if mu > 0:
        Wf = flip_ints(Wf, 8, mu, rng)
        if scheme.startswith("abft"):
            Cf = flip_ints(Cf, 16, mu, rng)
    result = np.zeros((B, P), dtype=np.int64)
    failed = np.zeros(B, dtype=bool)
    steps = np.zeros(B, dtype=np.int64)
    active = np.arange(B)
    for attempt in range(R + 1):
        if len(active) == 0:
            break
        if attempt > 0 and scheme in ("dwc", "abft"):          # reload from the clean master
            Wf[active] = Wclean
            Cf[active] = chk_clean
        a = len(active)
        acc = np.zeros((copies, a, lanes), dtype=np.int64)
        for k in range(n):
            xk = xb[active, k].astype(np.int64)[:, None]
            wl = Wf[active, :, k]                               # (a, P)
            if lanes > P:
                wl = np.concatenate([wl, Cf[active, k][:, None]], axis=1)
            acc += xk[None] * wl[None]
            if lam > 0:
                hit = rng.random((copies, a, lanes)) < lam
                bits = rng.integers(0, 32, size=(copies, a, lanes))
                acc ^= np.where(hit, (np.int64(1) << bits), 0)
            if force is not None and attempt == 0 and force[0] == k:
                acc[0, :, force[1]] ^= (1 << force[2])
            acc &= MASK
        steps[active] += n
        if scheme == "tmr":
            c0, c1, c2 = acc[0], acc[1], acc[2]
            maj = (c0 & c1) | (c0 & c2) | (c1 & c2)
            result[active] = to_signed32(maj)
            break
        if scheme == "none":
            result[active] = to_signed32(acc[0])
            break
        if scheme == "dwc":
            ok = (acc[0] == acc[1]).all(axis=1)
            res = acc[0]
        else:                                                   # abft variants
            ok = ((acc[0][:, :P].sum(axis=1) & MASK) == acc[0][:, P])
            res = acc[0][:, :P]
        result[active] = to_signed32(res)
        failed[active] = ~ok
        active = active[~ok]
    return result, failed, steps


def run_network(net, scheme, lam, mu, rng, B):
    """Run B images (xte cycled) through both layers. Returns per-image (pred, failed, steps)."""
    w1, b1, shift1, w2, b2, xte = net["w1"].astype(np.int64), net["b1"].astype(np.int64), int(net["shift1"]), \
        net["w2"].astype(np.int64), net["b2"].astype(np.int64), net["xte"]
    x = xte[np.arange(B) % len(xte)].astype(np.int64)
    failed = np.zeros(B, dtype=bool)
    steps = np.zeros(B, dtype=np.int64)
    acc1 = np.zeros((B, 32), dtype=np.int64)
    for r0 in range(0, 32, P):
        res, f, s = run_pass(x, w1[r0:r0 + P], scheme, lam, mu, rng)
        acc1[:, r0:r0 + P] = res
        failed |= f
        steps += s
    h = np.clip((acc1 + b1) >> shift1, 0, 127)
    logits = np.zeros((B, 10), dtype=np.int64)
    for r0 in range(0, 10, P):
        rows = w2[r0:r0 + P]
        Wp = np.zeros((P, 32), dtype=np.int64)
        Wp[:rows.shape[0]] = rows
        res, f, s = run_pass(h, Wp, scheme, lam, mu, rng)
        logits[:, r0:r0 + rows.shape[0]] = res[:, :rows.shape[0]]
        failed |= f
        steps += s
    logits += b2
    return logits.argmax(1), failed, steps


def sanity(net):
    rng = np.random.default_rng(0)
    B = len(net["xte"])
    gold, _, base_steps = run_network(net, "none", 0, 0, rng, B)
    acc_gold = float((gold == net["yte"]).mean())
    for sch in ("none", "dwc", "tmr", "abft", "abft_noreload"):
        pred, failed, steps = run_network(net, sch, 0, 0, rng, B)
        assert (pred == gold).all() and not failed.any(), f"{sch}: fault-free run differs from the reference"
    # a forced flip must equal the scalar model (which the Verilog matches)
    rng = np.random.default_rng(1)
    for t in range(200):
        n = int(rng.integers(1, 40))
        x = rng.integers(-128, 128, (1, n))
        W = rng.integers(-128, 128, (P, n))
        k, lane, bit = int(rng.integers(0, n)), int(rng.integers(0, P)), int(rng.integers(0, 32))
        got, _, _ = run_pass(x, W, "none", 0, 0, rng, force=(k, lane, bit))
        zero = np.zeros(P, dtype=np.uint64)
        exp = am.run_tile(x[0], W.T, np.arange(P), zero, zero, flip=(k, lane, bit))
        assert list(got[0]) == exp, "campaign model and scalar model disagree on a forced flip"
    return acc_gold


def cell(net, scheme, lam, mu, trials, seed):
    rng = np.random.default_rng(seed)
    n_img = len(net["xte"])
    B = n_img * trials
    gold_rng = np.random.default_rng(0)
    gold, _, base = run_network(net, "none", 0, 0, gold_rng, B)
    pred, failed, steps = run_network(net, scheme, lam, mu, rng, B)
    y = net["yte"][np.arange(B) % n_img]
    return dict(
        scheme=scheme, lam=lam, mu=mu, n=B,
        accuracy=float(((pred == y) & ~failed).mean()),             # a detected failure gives no answer
        silent_wrong=float(((pred != gold) & ~failed).mean()),      # wrong against the fault-free answer, undetected
        detected_fail=float(failed.mean()),
        cycles=float(steps.mean() / base.mean()),
    )


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--trials", type=int, default=5)
    ap.add_argument("--quick", action="store_true")
    args = ap.parse_args()
    net = np.load(os.path.join(ROOT, "results", "g19_network.npz"))
    acc_gold = sanity(net)
    print(f"sanity ok: fault-free accuracy {acc_gold:.4f}; every scheme reproduces the reference; forced flips match the scalar model")
    lams = [0, 1e-5, 1e-4, 3e-4, 1e-3, 3e-3, 1e-2]
    mus = [0, 1e-5, 1e-4, 3e-4, 1e-3, 3e-3, 1e-2, 3e-2]
    schemes = ["none", "dwc", "tmr", "abft", "abft_noreload"]
    trials = 1 if args.quick else args.trials
    rows = []
    for exp, grid in (("accumulator", [(l, 0) for l in lams]), ("weight", [(0, m) for m in mus])):
        for sch in schemes:
            for l, m in grid:
                r = cell(net, sch, l, m, trials, seed=zlib.crc32(repr((exp, sch, l, m)).encode()))
                r["experiment"] = exp
                rows.append(r)
                print(f"{exp:11s} {sch:14s} lam={l:g} mu={m:g}: acc {r['accuracy']:.3f} silent {r['silent_wrong']:.3f} "
                      f"detected {r['detected_fail']:.3f} cycles x{r['cycles']:.2f}")
    import csv
    with open(os.path.join(ROOT, "results", "g19_campaign.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=["experiment", "scheme", "lam", "mu", "n", "accuracy", "silent_wrong", "detected_fail", "cycles"])
        w.writeheader()
        w.writerows(rows)
    return acc_gold, rows


if __name__ == "__main__":
    main()
