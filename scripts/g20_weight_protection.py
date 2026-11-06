"""G20: weight-memory protection for the int8 digit classifier. The model and the predictions are fixed in
docs/preregistration-g20.md, which was committed before this script was first run.

Schemes (all rates are tier C and swept):
  none              no protection, never restored (a mission of N images)
  scrub S           restore from a clean copy every S images, no detection
  ecc               SECDED (13,8) per weight byte, correct on read, never restored; an uncorrectable word is a detected failure
  ecc_scrub S       as ecc, plus a restore every S images
  ecc_reload S      as ecc_scrub, but an uncorrectable word triggers a reload from the clean copy instead of a failure
  checksum_reload   G19 checksum per 8-row group each image; any mismatch reloads the clean copy (exposure one image)

usage: python scripts/g20_weight_protection.py [--trials T]
Writes results/g20_weight_protection.csv and results/g20_weight_protection.md.
"""
import argparse
import csv
import os
import zlib

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GOLD = 0.9685
FLOOR = GOLD - 0.01
DATA_POS = [3, 5, 6, 7, 9, 10, 11, 12]          # Hamming positions that carry data bits d0..d7


def build_secded_tables():
    """Exact extended-Hamming (13,8). Codeword bit 0 is the overall parity bit, bits 1..12 the Hamming positions.
    Returns (decoded byte, status) of shape (256, 8192) indexed by [data, 13-bit error mask];
    status 0 clean, 1 corrected, 2 detected as uncorrectable."""
    data = np.arange(256, dtype=np.int64)
    cw = np.zeros(256, dtype=np.int64)
    for i, pos in enumerate(DATA_POS):
        cw |= ((data >> i) & 1) << pos
    for k in range(4):                           # parity bits at positions 1, 2, 4, 8
        p = 1 << k
        par = np.zeros(256, dtype=np.int64)
        for pos in range(1, 13):
            if pos & p and pos != p:
                par ^= (cw >> pos) & 1
        cw |= par << p
    allpar = np.zeros(256, dtype=np.int64)
    for pos in range(1, 13):
        allpar ^= (cw >> pos) & 1
    cw |= allpar                                 # bit 0
    masks = np.arange(8192, dtype=np.int64)
    rx = cw[:, None] ^ masks[None, :]
    syn = np.zeros_like(rx)
    overall = np.zeros_like(rx)
    for pos in range(13):
        bit = (rx >> pos) & 1
        overall ^= bit
        if pos >= 1:
            syn ^= bit * pos
    status = np.where((syn == 0) & (overall == 0), 0, np.where((overall == 1) & (syn <= 12), 1, 2)).astype(np.uint8)
    fixed = np.where(status == 1, rx ^ (1 << syn), rx)
    out = np.zeros_like(rx)
    for i, pos in enumerate(DATA_POS):
        out |= ((fixed >> pos) & 1) << i
    # an error pattern that decodes to "clean" with a wrong byte is a silent corruption: the byte differs from the data
    return out.astype(np.uint8), status


def exposure_prob(mu, t):
    return (1.0 - (1.0 - 2.0 * mu) ** t) / 2.0


def flip_mask(shape, nbits, q, rng):
    """Independent flips of each of nbits bits with a per-element probability q (array broadcastable to shape)."""
    mask = np.zeros(shape, dtype=np.int64)
    for b in range(nbits):
        mask |= (rng.random(shape) < q).astype(np.int64) << b
    return mask


def network():
    net = np.load(os.path.join(ROOT, "results", "g19_network.npz"))
    return (net["w1"].astype(np.int64), net["b1"].astype(np.int64), int(net["shift1"]),
            net["w2"].astype(np.int64), net["b2"].astype(np.int64), net["xte"].astype(np.int64), net["yte"])


def predict(w1f, w2f, x, b1, shift1, b2):
    acc1 = np.einsum("bij,bj->bi", w1f, x) + b1
    h = np.clip(acc1 >> shift1, 0, 127)
    logits = np.einsum("bij,bj->bi", w2f, h) + b2
    return logits.argmax(1)


def groups_differ(wf, w):
    """True per image if any 8-row group's column sums differ from the clean ones (the G19 checksum)."""
    rows = w.shape[0]
    bad = np.zeros(wf.shape[0], dtype=bool)
    for r0 in range(0, rows, 8):
        bad |= (wf[:, r0:r0 + 8, :].sum(1) != w[r0:r0 + 8, :].sum(0)[None, :]).any(axis=1)
    return bad


def run_cell(scheme, S, mu, trials, tables, seed):
    w1, b1, shift1, w2, b2, xte, yte = network()
    n_img = len(xte)
    N = n_img * trials
    idx = np.arange(N)
    x = xte[idx % n_img]
    y = yte[idx % n_img]
    gold = predict(np.broadcast_to(w1, (N,) + w1.shape), np.broadcast_to(w2, (N,) + w2.shape), x, b1, shift1, b2)
    rng = np.random.default_rng(seed)
    if scheme == "checksum_reload":
        t = np.ones(N)
    elif S is None:
        t = 1.0 + idx
    else:
        t = 1.0 + (idx % S)
    q = exposure_prob(mu, t)
    dec, stat = tables
    pred = np.zeros(N, dtype=np.int64)
    failed = np.zeros(N, dtype=bool)
    reloaded = np.zeros(N, dtype=bool)
    chunk = 100
    for c0 in range(0, N, chunk):
        sl = slice(c0, min(N, c0 + chunk))
        n = sl.stop - sl.start
        qc = q[sl][:, None, None]
        ws, wsf = [], []
        bad_img = np.zeros(n, dtype=bool)
        silent_flag = np.zeros(n, dtype=bool)
        for w in (w1, w2):
            u8 = (w & 0xFF).astype(np.int64)
            if scheme in ("none", "scrub", "checksum_reload"):
                wf_u8 = u8[None] ^ flip_mask((n,) + w.shape, 8, qc, rng)
                wf = np.where(wf_u8 >= 128, wf_u8 - 256, wf_u8)
                if scheme == "checksum_reload":
                    bad_img |= groups_differ(wf, w)
            else:                                                  # ecc variants
                m = flip_mask((n,) + w.shape, 13, qc, rng)
                d = np.broadcast_to(u8[None], m.shape)
                got = dec[d, m].astype(np.int64)
                st = stat[d, m]
                bad_img |= (st == 2).reshape(n, -1).any(axis=1)
                wf = np.where(got >= 128, got - 256, got)
            wsf.append(wf)
        p = predict(wsf[0], wsf[1], x[sl], b1, shift1, b2)
        if scheme == "checksum_reload":
            clean = predict(np.broadcast_to(w1, (n,) + w1.shape), np.broadcast_to(w2, (n,) + w2.shape), x[sl], b1, shift1, b2)
            p = np.where(bad_img, clean, p)
            reloaded[sl] = bad_img
        elif scheme in ("ecc", "ecc_scrub"):
            failed[sl] = bad_img
        elif scheme == "ecc_reload":
            clean = predict(np.broadcast_to(w1, (n,) + w1.shape), np.broadcast_to(w2, (n,) + w2.shape), x[sl], b1, shift1, b2)
            p = np.where(bad_img, clean, p)
            reloaded[sl] = bad_img
        pred[sl] = p
    return dict(scheme=scheme, S=("inf" if S is None else S), mu=mu, n=N,
                accuracy=float(((pred == y) & ~failed).mean()),
                silent_wrong=float(((pred != gold) & ~failed).mean()),
                detected_fail=float(failed.mean()), reload_rate=float(reloaded.mean()))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--trials", type=int, default=5)
    args = ap.parse_args()
    tables = build_secded_tables()
    # sanity: the code corrects every single error and detects every double error, for every data byte
    dec, stat = tables
    data = np.arange(256)
    for b in range(13):
        assert (dec[data, 1 << b] == data).all() and (stat[data, 1 << b] == 1).all()
    for a in range(13):
        for b in range(a + 1, 13):
            assert (stat[data, (1 << a) | (1 << b)] == 2).all()
    assert (stat[data, 0] == 0).all() and (dec[data, 0] == data).all()
    print("SECDED sanity ok: all single errors corrected, all double errors detected")
    mus = [1e-6, 1e-5, 1e-4, 1e-3]
    plan = [("none", None), ("scrub", 100), ("scrub", 10), ("ecc", None), ("ecc_scrub", 100), ("ecc_scrub", 10),
            ("ecc_reload", 100), ("checksum_reload", None)]
    rows = []
    for scheme, S in plan:
        for mu in mus:
            r = run_cell(scheme, S, mu, args.trials, tables, zlib.crc32(repr((scheme, S, mu)).encode()))
            rows.append(r)
            print(f"{scheme:16s} S={str(r['S']):4s} mu={mu:g}: acc {r['accuracy']:.3f} silent {r['silent_wrong']:.3f} "
                  f"detected {r['detected_fail']:.3f} reload {r['reload_rate']:.3f}")
    with open(os.path.join(ROOT, "results", "g20_weight_protection.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=["scheme", "S", "mu", "n", "accuracy", "silent_wrong", "detected_fail", "reload_rate"])
        w.writeheader()
        w.writerows(rows)
    out = ["# G20: weight-memory protection (results)", "",
           f"Model and predictions: `docs/preregistration-g20.md`. 2,700 images per cell; floor {FLOOR:.4f}. "
           "Cell: accuracy / silent wrong / detected failure / reload rate.", "",
           "| scheme | S | " + " | ".join(f"mu={m:g}" for m in mus) + " |", "|---|---|" + "---|" * len(mus)]
    for scheme, S in plan:
        cells = []
        for mu in mus:
            r = next(r for r in rows if r["scheme"] == scheme and r["S"] == ("inf" if S is None else S) and r["mu"] == mu)
            cells.append(f"{r['accuracy']:.3f} / {r['silent_wrong']:.3f} / {r['detected_fail']:.3f} / {r['reload_rate']:.3f}")
        out.append(f"| {scheme} | {'inf' if S is None else S} | " + " | ".join(cells) + " |")
    with open(os.path.join(ROOT, "results", "g20_weight_protection.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(out))


if __name__ == "__main__":
    main()
