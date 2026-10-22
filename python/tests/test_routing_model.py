import math
import unittest

from radslice import cluster_model as cm
from radslice import cost_model as cost
from radslice import routing_model as rm

P = 0.71
Q_REG = 0.99762
Q_DEV = 0.9992


class TestHeteroYield(unittest.TestCase):
    def test_equal_yields_match_the_pooled_model(self):
        for s in (0, 16, 100, 140):
            self.assertAlmostEqual(rm.hetero_yield(256, P, s, P), cm.pooled_yield(256, s, P), places=10)

    def test_brute_force_small_case(self):
        # 2 primaries (p = 0.9), 2 spares (0.6): enumerate
        p, ps = 0.9, 0.6
        want = 0.0
        for a in (0, 1):
            for b in (0, 1):
                for c in (0, 1):
                    for d in (0, 1):
                        pr = (p if a else 1 - p) * (p if b else 1 - p) * (ps if c else 1 - ps) * (ps if d else 1 - ps)
                        bad = (1 - a) + (1 - b)
                        if c + d >= bad:
                            want += pr
        self.assertAlmostEqual(rm.hetero_yield(2, p, 2, ps), want, places=12)

    def test_worse_spares_lower_the_yield(self):
        self.assertGreater(rm.hetero_yield(256, P, 150, P), rm.hetero_yield(256, P, 150, 0.3))

    def test_no_spares(self):
        self.assertAlmostEqual(rm.hetero_yield(10, 0.9, 0, 0.5), 0.9 ** 10, places=12)


class TestRouting(unittest.TestCase):
    def test_perfect_transistors_reproduce_g10(self):
        for regions, spares in ((1, 128), (8, 192), (4, 160)):
            d = 256 // regions
            if regions == 1:
                want = cm.pooled_yield(256, spares, P)
            else:
                want = cm.domain_yield(regions, d, spares // regions, P)
            self.assertAlmostEqual(rm.rf_yield(P, 1.0, spares, regions), want, places=9)

    def test_private_routing_per_spare_at_the_g10_optimum(self):
        # 8 regions, 192 spares: about 414 private transistors per spare
        self.assertAlmostEqual(rm.spare_private_T(192, 8), 414.0, places=0)
        self.assertAlmostEqual(P * Q_REG ** 414, 0.2652, places=3)

    def test_routing_defects_lower_yield_and_more_so_at_the_lower_q(self):
        y1 = rm.rf_yield(P, 1.0, 192, 8)
        y2 = rm.rf_yield(P, Q_DEV, 192, 8)
        y3 = rm.rf_yield(P, Q_REG, 192, 8)
        self.assertGreater(y1, y2)
        self.assertGreater(y2, y3)

    def test_shared_block_needs_replication(self):
        one = Q_REG ** rm.SHARED_T
        self.assertAlmostEqual(one, 0.0643, places=3)
        self.assertGreater(rm.rf_yield(P, Q_REG, 192, 8, copies=4), rm.rf_yield(P, Q_REG, 192, 8, copies=1))

    def test_study_headline_numbers(self):
        # cheapest tagged design with only the spares' private routing counted (results/routing_defect_study.md)
        base = cost.RF_PRIMARIES * cost.T_RF_BLOCK
        for q, spares, want in ((1.0, 192, 4.21), (Q_DEV, 280, 5.72), (Q_REG, 568, 10.58)):
            y = rm.rf_core_yield(P, q, spares, 8)
            t = rm.rf_silicon_T(spares, 8, 1, shared_T=0.0)
            self.assertAlmostEqual(t / y / base, want, places=2)

    def test_alu_single_unit(self):
        y_ideal = rm.alu_yield(P, 1.0, 8, 2, "single")
        self.assertAlmostEqual(y_ideal, 1 - (1 - P) ** 3, places=12)
        self.assertLess(rm.alu_yield(P, Q_REG, 8, 2, "single"), y_ideal)


if __name__ == "__main__":
    unittest.main()
