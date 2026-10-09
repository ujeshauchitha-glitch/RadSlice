import unittest

from radslice import yield_model as ym
from radslice.params import load_params

P = 0.71


class ClosedForm(unittest.TestCase):
    def test_64bit_register_no_spares(self):
        # 8 byte slices at 0.71 each: 0.71**8 = 0.0646 (project brief).
        self.assertAlmostEqual(ym.register_yield(8, P), 0.0646, places=4)

    def test_binom_tail_edges(self):
        self.assertEqual(ym.binom_tail(10, 0, P), 1.0)
        self.assertEqual(ym.binom_tail(10, 11, P), 0.0)
        self.assertAlmostEqual(ym.binom_tail(10, 10, P), P**10, places=12)

    def test_pool_equals_p_pow_n_without_spares(self):
        self.assertAlmostEqual(ym.file_yield_shared_pool(256, 0, P), P**256, places=300)

    def test_spare_table(self):
        rows = {s: (a, pr, sp) for s, a, pr, sp in ym.spare_table(P)}
        a, pr, sp = rows[128]
        self.assertAlmostEqual(a, 1.5)
        self.assertAlmostEqual(sp, 0.9718, places=3)   # "about 97%" in the brief
        self.assertLess(pr, 1e-3)                       # per-register far worse
        self.assertLess(rows[96][2], 0.5)               # sharp cliff below 128
        self.assertIsNone(rows[16][1])                  # 16 does not split over 32 regs

    def test_pool_yield_monotone_in_spares(self):
        ys = [ym.file_yield_shared_pool(256, s, P) for s in range(0, 160, 8)]
        self.assertEqual(ys, sorted(ys))


class MonteCarlo(unittest.TestCase):
    def test_pool_matches_closed_form_within_4_sigma(self):
        exact = ym.file_yield_shared_pool(256, 128, P)
        est, se = ym.mc_shared_pool(256, 128, P, trials=20000, seed=12345)
        self.assertLess(abs(est - exact), 4 * se)


class Params(unittest.TestCase):
    def test_all_tier_c_for_now(self):
        params = load_params()
        self.assertTrue(params)
        for p in params.values():
            self.assertEqual(p.tier, "C", p.name)

    def test_yield_param_matches_test_constant(self):
        self.assertEqual(load_params()["yield.p_slice_good"].value, P)

    def test_tier_a_requires_source(self):
        import tempfile, pathlib
        from radslice.params import load_params as lp
        with tempfile.TemporaryDirectory() as d:
            f = pathlib.Path(d) / "p.toml"
            f.write_text('[x]\nvalue = 1\ntier = "A"\n')
            with self.assertRaises(ValueError):
                lp(f)
            f.write_text('[x]\nvalue = 1\ntier = "Z"\n')
            with self.assertRaises(ValueError):
                lp(f)


if __name__ == "__main__":
    unittest.main()
