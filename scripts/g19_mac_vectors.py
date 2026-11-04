"""G19 step 2: test vectors for rtl/mac_array.v, from the integer reference (no hardware model involved).

Writes lines for the testbench: 'T n' then n lines 'x w0..w{P-1}' then one line of P expected int32 sums.
Cases: random tiles with extreme values, and the real network (layer 1 in passes of P lanes over held-out
images, layer 2 likewise), so the array is checked on the work it is for.
usage: g19_mac_vectors.py P ntiles > file
"""
import os
import sys
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def emit(out, xs, wmat):
    """xs: (n,) ints; wmat: (n, P) ints. Expected = sum_k xs[k] * wmat[k, :] in exact integers."""
    n, p = wmat.shape
    exp = (xs.astype(np.int64)[:, None] * wmat.astype(np.int64)).sum(0)
    assert np.abs(exp).max() < 2 ** 31
    out.append(f"T {n}")
    for k in range(n):
        out.append(" ".join(str(int(v)) for v in [xs[k], *wmat[k]]))
    out.append(" ".join(str(int(v)) for v in exp))


def main():
    P = int(sys.argv[1])
    ntiles = int(sys.argv[2])
    rng = np.random.default_rng(19)
    out = []
    for t in range(ntiles):
        n = int(rng.integers(1, 80))
        xs = rng.integers(-128, 128, n)
        wm = rng.integers(-128, 128, (n, P))
        if t % 5 == 0:
            xs[:] = rng.choice([-128, 127], n)
            wm[:] = rng.choice([-128, 127], (n, P))
        emit(out, xs, wm)
    net = np.load(os.path.join(ROOT, "results", "g19_network.npz"))
    w1, w2, xte = net["w1"].astype(np.int64), net["w2"].astype(np.int64), net["xte"]
    shift1, b1 = int(net["shift1"]), net["b1"].astype(np.int64)
    for img in range(20):
        x = xte[img].astype(np.int64)
        for r0 in range(0, w1.shape[0], P):
            rows = w1[r0:r0 + P]
            wm = np.zeros((64, P), dtype=np.int64)
            wm[:, :rows.shape[0]] = rows.T
            emit(out, x, wm)
        h = np.clip((w1 @ x + b1) >> shift1, 0, 127)
        for r0 in range(0, w2.shape[0], P):
            rows = w2[r0:r0 + P]
            wm = np.zeros((w2.shape[1], P), dtype=np.int64)
            wm[:, :rows.shape[0]] = rows.T
            emit(out, h, wm)
    print("\n".join(out))


if __name__ == "__main__":
    main()
