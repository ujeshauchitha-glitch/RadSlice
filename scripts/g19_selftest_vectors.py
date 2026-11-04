"""G19 step 3: defective-chip files for rtl/tb/tb_mac_selftest.v.

usage: g19_selftest_vectors.py P S nchips qdef seed NRAW  > chips.txt   (tiles come from g19_mac_vectors.py)

Per chip: PH physical elements, each defective with probability qdef, a defective element having ONE stuck bit
(random bit 0..31, random value). The file gives, per chip:
  C <accept>                    accept = 1 if the number of defective elements is at most S (repairable by remap)
  PH lines '<mask hex> <val hex>'
  NRAW lines of P results       the faulty array's outputs with the identity map on the first NRAW tiles
  F <tile> <step> <element> <bit> then P results    a flip on a fault-free array (the flip hook)
"""
import sys
import numpy as np
import g19_array_model as am
import g19_mac_vectors as mv


def main():
    P, S, nchips = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3])
    qdef, seed, nraw = float(sys.argv[4]), int(sys.argv[5]), int(sys.argv[6])
    ph = P + S
    rng = np.random.default_rng(seed)
    tiles = mv.make_tiles(P, 40)[:nraw + 1]
    out = []
    ident = np.arange(P)
    for c in range(nchips):
        mask = np.zeros(ph, dtype=np.uint64)
        val = np.zeros(ph, dtype=np.uint64)
        ndef = 0
        for e in range(ph):
            if rng.random() < qdef:
                b = int(rng.integers(0, 32))
                mask[e] = 1 << b
                val[e] = (1 << b) if rng.random() < 0.5 else 0
                ndef += 1
        out.append(f"C {1 if ndef <= S else 0}")
        for e in range(ph):
            out.append(f"{int(mask[e]):x} {int(val[e]):x}")
        for t in range(nraw):
            xs, wm = tiles[t]
            out.append(" ".join(str(v) for v in am.run_tile(xs, wm, ident, mask, val)))
        xs, wm = tiles[nraw]
        step = int(rng.integers(0, len(xs)))
        el = int(rng.integers(0, ph))
        bit = int(rng.integers(0, 32))
        zero = np.zeros(ph, dtype=np.uint64)
        out.append(f"F {nraw} {step} {el} {bit}")
        out.append(" ".join(str(v) for v in am.run_tile(xs, wm, np.arange(P), zero, zero, flip=(step, el, bit))))
    print("\n".join(out))


if __name__ == "__main__":
    main()
